#include "Framework/Render/Public/ForwardShadows.h"

#include "Framework/Asset/Public/AssetManager.h"
#include "Framework/Asset/Public/MeshAsset.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIDefinitions.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "Framework/RHI/Public/RHITexture.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>

namespace ShadowEngine
{
    namespace
    {
        constexpr uint32 DirectionalShadowSize = 2048;
        constexpr uint32 PointShadowFaceSize = 1024;
        constexpr uint32 ShadowConstantSlots = 512;
        constexpr uint32 ShadowConstantStride = 1024;
        constexpr float PointDepthBias = 0.004F;

        struct GpuShadowConstants
        {
            float ViewProjection[16];
            float World[16];
            float LightPosition[4];
        };

        struct ShadowDraw
        {
            const MeshBatch* Batch = nullptr;
            float ViewProjection[16] = {};
            float World[16] = {};
            float LightPosition[4] = {};
            float NormalMatrix[16] = {};
            RHITexture* Target = nullptr;
            RHIPipeline* Pipeline = nullptr;
            uint32 Face = 0;
            uint32 Size = 0;
            bool bBegin = false;
            bool bClip = false;
        };

        glm::vec3 TransformPoint(const float World[16], float X, float Y, float Z)
        {
            const glm::vec4 Result = glm::make_mat4(World) * glm::vec4(X, Y, Z, 1.0F);
            return glm::vec3(Result);
        }

        bool FitDirectionalClip(
            const std::vector<MeshBatch>& Batches,
            const std::vector<MeshRenderProxy>& Casters,
            const AssetManager& Assets,
            const DirectLightRenderProxy& Light,
            float OutClip[16],
            float& OutWorldTexel)
        {
            const glm::vec3 RawDirection(Light.Direction[0], Light.Direction[1], Light.Direction[2]);
            if (glm::length(RawDirection) < 0.0001F)
            {
                return false;
            }

            const glm::vec3 Direction = glm::normalize(RawDirection);
            bool bAny = false;
            glm::vec3 BoundsMin(0.0F);
            glm::vec3 BoundsMax(0.0F);
            for (const MeshRenderProxy& Caster : Casters)
            {
                const MeshAsset* Mesh = Assets.ResolveMesh(Caster.Mesh);
                if (Mesh == nullptr)
                {
                    continue;
                }

                bool bDrawn = false;
                for (const MeshBatch& Batch : Batches)
                {
                    if (Batch.Mesh.Index == Caster.Mesh.Index)
                    {
                        bDrawn = true;
                        break;
                    }
                }
                if (!bDrawn)
                {
                    continue;
                }

                const std::vector<MeshSection>& Sections = Mesh->GetSections();
                for (uint32 Section = 0; Section < Sections.size(); ++Section)
                {
                    const MeshBatch* SectionBatch = FindMeshBatch(Batches, Caster.Mesh, Section);
                    if (SectionBatch == nullptr || SectionBatch->Surface == EMaterialSurface::Translucent)
                    {
                        continue;
                    }

                    const MeshSection& Bounds = Sections[Section];
                    for (uint32 Corner = 0; Corner < 8; ++Corner)
                    {
                        const glm::vec3 Point = TransformPoint(
                            Caster.World,
                            (Corner & 1U) != 0U ? Bounds.BoundsMax[0] : Bounds.BoundsMin[0],
                            (Corner & 2U) != 0U ? Bounds.BoundsMax[1] : Bounds.BoundsMin[1],
                            (Corner & 4U) != 0U ? Bounds.BoundsMax[2] : Bounds.BoundsMin[2]);
                        if (!bAny)
                        {
                            BoundsMin = Point;
                            BoundsMax = Point;
                            bAny = true;
                        }
                        else
                        {
                            BoundsMin = glm::min(BoundsMin, Point);
                            BoundsMax = glm::max(BoundsMax, Point);
                        }
                    }
                }
            }
            if (!bAny)
            {
                return false;
            }

            const glm::vec3 Center = (BoundsMin + BoundsMax) * 0.5F;
            const float Radius = glm::length(BoundsMax - BoundsMin) * 0.5F + 1.0F;
            const glm::vec3 Eye = Center - Direction * Radius;
            const glm::vec3 Up = std::abs(Direction.y) > 0.99F ? glm::vec3(0.0F, 0.0F, 1.0F) : glm::vec3(0.0F, 1.0F, 0.0F);
            const glm::mat4 LightView = glm::lookAtRH(Eye, Center, Up);

            bool bView = false;
            glm::vec3 ViewMin(0.0F);
            glm::vec3 ViewMax(0.0F);
            for (const MeshRenderProxy& Caster : Casters)
            {
                const MeshAsset* Mesh = Assets.ResolveMesh(Caster.Mesh);
                if (Mesh == nullptr)
                {
                    continue;
                }

                const std::vector<MeshSection>& Sections = Mesh->GetSections();
                for (uint32 Section = 0; Section < Sections.size(); ++Section)
                {
                    const MeshBatch* SectionBatch = FindMeshBatch(Batches, Caster.Mesh, Section);
                    if (SectionBatch == nullptr || SectionBatch->Surface == EMaterialSurface::Translucent)
                    {
                        continue;
                    }

                    const MeshSection& Bounds = Sections[Section];
                    for (uint32 Corner = 0; Corner < 8; ++Corner)
                    {
                        const glm::vec3 WorldPoint = TransformPoint(
                            Caster.World,
                            (Corner & 1U) != 0U ? Bounds.BoundsMax[0] : Bounds.BoundsMin[0],
                            (Corner & 2U) != 0U ? Bounds.BoundsMax[1] : Bounds.BoundsMin[1],
                            (Corner & 4U) != 0U ? Bounds.BoundsMax[2] : Bounds.BoundsMin[2]);
                        const glm::vec3 ViewPoint = glm::vec3(LightView * glm::vec4(WorldPoint, 1.0F));
                        if (!bView)
                        {
                            ViewMin = ViewPoint;
                            ViewMax = ViewPoint;
                            bView = true;
                        }
                        else
                        {
                            ViewMin = glm::min(ViewMin, ViewPoint);
                            ViewMax = glm::max(ViewMax, ViewPoint);
                        }
                    }
                }
            }
            if (!bView)
            {
                return false;
            }

            const float PadX = std::max((ViewMax.x - ViewMin.x) * 0.05F, 0.25F);
            const float PadY = std::max((ViewMax.y - ViewMin.y) * 0.05F, 0.25F);
            const float Near = std::max(0.05F, -ViewMax.z - 1.0F);
            float Far = -ViewMin.z + 1.0F;
            if (Far <= Near)
            {
                Far = Near + 1.0F;
            }

            const glm::mat4 Clip = glm::orthoRH_ZO(
                ViewMin.x - PadX,
                ViewMax.x + PadX,
                ViewMin.y - PadY,
                ViewMax.y + PadY,
                Near,
                Far) * LightView;
            const float ExtentX = (ViewMax.x - ViewMin.x) + PadX * 2.0F;
            const float ExtentY = (ViewMax.y - ViewMin.y) + PadY * 2.0F;
            OutWorldTexel = std::max(ExtentX, ExtentY) / static_cast<float>(DirectionalShadowSize);
            std::memcpy(OutClip, glm::value_ptr(Clip), sizeof(float) * 16);
            return true;
        }

        bool AppendCasters(
            std::vector<ShadowDraw>& Draws,
            const std::vector<MeshBatch>& Batches,
            const std::vector<MeshRenderProxy>& Casters,
            const float ViewProjection[16],
            const float LightPosition[4],
            RHITexture& Target,
            RHIPipeline& BackPipeline,
            RHIPipeline& TwoSidedPipeline,
            bool bPoint,
            uint32 Face,
            uint32 Size)
        {
            bool bFirst = true;
            for (const MeshRenderProxy& Caster : Casters)
            {
                for (const MeshBatch& Batch : Batches)
                {
                    if (Batch.Mesh.Index != Caster.Mesh.Index || Batch.Surface == EMaterialSurface::Translucent ||
                        Batch.VertexBuffer == nullptr || Batch.IndexBuffer == nullptr)
                    {
                        continue;
                    }

                    RHIPipeline* Pipeline = Batch.bTwoSided ? &TwoSidedPipeline : &BackPipeline;
                    const bool bClip = Batch.Surface == EMaterialSurface::Cutout;
                    if (bClip)
                    {
                        Pipeline = bPoint ? Batch.PointShadowPipeline.get() : Batch.DirectionalShadowPipeline.get();
                        if (Pipeline == nullptr)
                        {
                            Log::Error("Cutout shadow pipeline is missing");
                            return false;
                        }
                    }

                    ShadowDraw Draw;
                    Draw.Batch = &Batch;
                    std::memcpy(Draw.ViewProjection, ViewProjection, sizeof(Draw.ViewProjection));
                    std::memcpy(Draw.World, Caster.World, sizeof(Draw.World));
                    std::memcpy(Draw.NormalMatrix, Caster.NormalMatrix, sizeof(Draw.NormalMatrix));
                    std::memcpy(Draw.LightPosition, LightPosition, sizeof(Draw.LightPosition));
                    Draw.Target = &Target;
                    Draw.Pipeline = Pipeline;
                    Draw.Face = Face;
                    Draw.Size = Size;
                    Draw.bBegin = bFirst;
                    Draw.bClip = bClip;
                    bFirst = false;
                    Draws.push_back(Draw);
                }
            }

            if (bFirst)
            {
                ShadowDraw Clear;
                Clear.Target = &Target;
                Clear.Pipeline = &BackPipeline;
                Clear.Face = Face;
                Clear.Size = Size;
                Clear.bBegin = true;
                Draws.push_back(Clear);
            }

            return true;
        }
    }

    std::unique_ptr<RHIPipeline> ForwardShadows::CreateDepthPipeline(
        RHIDevice& Device,
        ShaderManager& Shaders,
        const char* ShaderPath,
        int32 DepthBias,
        float SlopeScaledDepthBias,
        ERHICullMode Cull,
        std::string* ErrorMessage)
    {
        const ShaderHandle Shader = Shaders.Register(ShaderPath, ShaderPath, {}, {});
        if (!Shader.IsValid() || !Shaders.LoadShaders(Device, Shader, ErrorMessage))
        {
            return nullptr;
        }

        const ShaderReflection* VertexReflection = Shaders.GetReflection(Shader, ERHIShaderStage::Vertex);
        const ShaderReflection* PixelReflection = Shaders.GetReflection(Shader, ERHIShaderStage::Pixel);
        const RHIPipelineLayout Layout = VertexReflection != nullptr && PixelReflection != nullptr
            ? RHIPipelineLayout::FromReflection(*VertexReflection, *PixelReflection)
            : RHIPipelineLayout{};
        constexpr RHIInputElement InputLayout[] = {
            {"POSITION", 0, 0, ERHIVertexFormat::Float32x3},
        };
        RHIGraphicsPipelineDesc PipelineDesc;
        PipelineDesc.VertexShader = Shaders.GetVertexShader(Shader);
        PipelineDesc.PixelShader = Shaders.GetPixelShader(Shader);
        PipelineDesc.Layout = &Layout;
        PipelineDesc.RenderTargetFormat = ERHIFormat::Unknown;
        PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
        PipelineDesc.InputLayout = InputLayout;
        PipelineDesc.bEnableDepth = true;
        PipelineDesc.bWriteDepth = true;
        PipelineDesc.bDepthEqual = false;
        PipelineDesc.bWriteColor = false;
        PipelineDesc.DepthFormat = ERHIFormat::D32_Float;
        PipelineDesc.DepthBias = DepthBias;
        PipelineDesc.SlopeScaledDepthBias = SlopeScaledDepthBias;
        PipelineDesc.Cull = Cull;
        PipelineDesc.bFrontCounterClockwise = true;
        return Device.CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
    }

    bool ForwardShadows::Ensure(RHIDevice& Device, ShaderManager& Shaders, std::string* ErrorMessage)
    {
        static_assert(sizeof(GpuShadowConstants) == 144);
        static_assert(sizeof(GpuShadowConstants) <= ShadowConstantStride);
        if (bReady)
        {
            return true;
        }

        DirectionalPipeline = CreateDepthPipeline(Device, Shaders, "Depth/DirectionalShadow.slang", 0, 0.0F, ERHICullMode::Back, ErrorMessage);
        DirectionalTwoSidedPipeline = CreateDepthPipeline(
            Device,
            Shaders,
            "Depth/DirectionalShadow.slang",
            0,
            0.0F,
            ERHICullMode::None,
            ErrorMessage);
        PointPipeline = CreateDepthPipeline(Device, Shaders, "Depth/PointShadow.slang", 0, 0.0F, ERHICullMode::Back, ErrorMessage);
        PointTwoSidedPipeline = CreateDepthPipeline(Device, Shaders, "Depth/PointShadow.slang", 0, 0.0F, ERHICullMode::None, ErrorMessage);
        if (DirectionalPipeline == nullptr || DirectionalTwoSidedPipeline == nullptr || PointPipeline == nullptr ||
            PointTwoSidedPipeline == nullptr)
        {
            return false;
        }

        DirectionalMap = Device.CreateDepthTexture(DirectionalShadowSize, DirectionalShadowSize, ErrorMessage);
        if (DirectionalMap == nullptr)
        {
            return false;
        }

        for (uint32 Index = 0; Index < 4; ++Index)
        {
            PointMaps[Index] = Device.CreateCubeDepth(PointShadowFaceSize, ErrorMessage);
            if (PointMaps[Index] == nullptr)
            {
                return false;
            }
        }

        ComparisonSampler = Device.CreateComparisonSampler(ErrorMessage);
        ConstantBuffer = Device.CreateConstantBuffer(ShadowConstantSlots * ShadowConstantStride, ErrorMessage);
        if (ComparisonSampler == nullptr || ConstantBuffer == nullptr)
        {
            return false;
        }

        if (!Device.CreateShaderResourceView(*DirectionalMap, DirectDescriptor, ErrorMessage))
        {
            return false;
        }
        for (uint32 Index = 0; Index < 4; ++Index)
        {
            if (!Device.CreateShaderResourceView(*PointMaps[Index], PointDescriptors[Index], ErrorMessage))
            {
                return false;
            }
        }

        ComparisonDescriptor = ComparisonSampler->GetDescriptorIndex();
        Binding.ComparisonSampler = ComparisonDescriptor;
        Binding.Settings[0] = 1.0F / static_cast<float>(DirectionalShadowSize);
        Binding.Settings[1] = 0.02F;
        Binding.Settings[2] = 2.0F / static_cast<float>(PointShadowFaceSize);
        Binding.Settings[3] = PointDepthBias;
        bReady = true;
        Log::Info(
            "PCF shadows ready: directional {}, point cube {} per face",
            DirectionalShadowSize,
            PointShadowFaceSize);
        return true;
    }

    bool ForwardShadows::Prepare(
        const std::vector<MeshBatch>& Batches,
        const std::vector<MeshRenderProxy>& Casters,
        const AssetManager& Assets,
        const DirectLightRenderProxy* DirectLight,
        const std::vector<PointLightRenderProxy>& PointLights,
        std::string* ErrorMessage)
    {
        if (!bReady)
        {
            SetErrorMessage(ErrorMessage, "Shadow maps are not ready");
            return false;
        }

        bDirectActive = false;
        for (bool& Active : bPointActive)
        {
            Active = false;
        }

        const float Settings[4] = {
            1.0F / static_cast<float>(DirectionalShadowSize),
            0.02F,
            2.0F / static_cast<float>(PointShadowFaceSize),
            PointDepthBias};
        Binding = {};
        Binding.DirectMap = InvalidShaderDescriptor;
        Binding.ComparisonSampler = ComparisonDescriptor;
        std::memcpy(Binding.Settings, Settings, sizeof(Settings));
        for (uint32 Index = 0; Index < 4; ++Index)
        {
            Binding.PointMap[Index] = InvalidShaderDescriptor;
        }

        float WorldTexel = 0.0F;
        if (DirectLight != nullptr && DirectLight->Intensity > 0.0F &&
            FitDirectionalClip(Batches, Casters, Assets, *DirectLight, DirectClip, WorldTexel))
        {
            const glm::mat4 Clip = glm::make_mat4(DirectClip);
            for (int Row = 0; Row < 4; ++Row)
            {
                Binding.DirectRow[Row][0] = Clip[0][Row];
                Binding.DirectRow[Row][1] = Clip[1][Row];
                Binding.DirectRow[Row][2] = Clip[2][Row];
                Binding.DirectRow[Row][3] = Clip[3][Row];
            }
            Binding.DirectMap = DirectDescriptor;
            Binding.Settings[1] = std::max(WorldTexel * 3.0F, 0.01F);
            bDirectActive = true;
        }

        const size_t PointCount = std::min(PointLights.size(), static_cast<size_t>(4));
        for (size_t Index = 0; Index < PointCount; ++Index)
        {
            if (PointLights[Index].Intensity <= 0.0F || PointLights[Index].Radius <= 0.0F)
            {
                continue;
            }

            ActivePoints[Index] = PointLights[Index];
            Binding.PointMap[Index] = PointDescriptors[Index];
            bPointActive[Index] = true;
        }
        return true;
    }

    bool ForwardShadows::Record(
        RHICommandList& Commands,
        const std::vector<MeshBatch>& Batches,
        const std::vector<MeshRenderProxy>& Casters)
    {
        if (!bReady || ConstantBuffer == nullptr)
        {
            Log::Error("Shadow maps are not ready");
            return false;
        }

        std::vector<ShadowDraw> Draws;
        if (bDirectActive && DirectionalMap != nullptr && DirectionalPipeline != nullptr && DirectionalTwoSidedPipeline != nullptr)
        {
            const float UnusedLight[4] = {};
            if (!AppendCasters(
                    Draws,
                    Batches,
                    Casters,
                    DirectClip,
                    UnusedLight,
                    *DirectionalMap,
                    *DirectionalPipeline,
                    *DirectionalTwoSidedPipeline,
                    false,
                    0,
                    DirectionalShadowSize))
            {
                return false;
            }
        }

        static const glm::vec3 FaceDirection[6] = {
            {1.0F, 0.0F, 0.0F},
            {-1.0F, 0.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
            {0.0F, -1.0F, 0.0F},
            {0.0F, 0.0F, 1.0F},
            {0.0F, 0.0F, -1.0F},
        };
        static const glm::vec3 FaceUp[6] = {
            {0.0F, 1.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
            {0.0F, 0.0F, -1.0F},
            {0.0F, 0.0F, 1.0F},
            {0.0F, 1.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
        };

        for (uint32 LightIndex = 0; LightIndex < 4; ++LightIndex)
        {
            if (!bPointActive[LightIndex] || PointMaps[LightIndex] == nullptr || PointPipeline == nullptr)
            {
                continue;
            }

            const PointLightRenderProxy& Point = ActivePoints[LightIndex];
            const glm::vec3 Eye(Point.Position[0], Point.Position[1], Point.Position[2]);
            const float Far = std::max(Point.Radius, 0.1F);
            const glm::mat4 Projection = glm::perspectiveRH_ZO(glm::radians(90.0F), 1.0F, 0.05F, Far);
            const float LightPosition[4] = {Point.Position[0], Point.Position[1], Point.Position[2], Point.Radius};
            for (uint32 Face = 0; Face < 6; ++Face)
            {
                const glm::mat4 Clip = Projection * glm::lookAtRH(Eye, Eye + FaceDirection[Face], FaceUp[Face]);
                float ViewProjection[16];
                std::memcpy(ViewProjection, glm::value_ptr(Clip), sizeof(ViewProjection));
                if (PointTwoSidedPipeline == nullptr ||
                    !AppendCasters(
                        Draws,
                        Batches,
                        Casters,
                        ViewProjection,
                        LightPosition,
                        *PointMaps[LightIndex],
                        *PointPipeline,
                        *PointTwoSidedPipeline,
                        true,
                        Face,
                        PointShadowFaceSize))
                {
                    return false;
                }
            }
        }

        if (Draws.size() > ShadowConstantSlots)
        {
            Log::Error("Shadow constant buffer is full");
            return false;
        }

        std::vector<uint8> Constants(ShadowConstantSlots * ShadowConstantStride);
        for (size_t Index = 0; Index < Draws.size(); ++Index)
        {
            uint8* Destination = Constants.data() + Index * ShadowConstantStride;
            if (Draws[Index].bClip)
            {
                const MeshBatch* Batch = Draws[Index].Batch;
                if (Batch == nullptr || Batch->ShadowMaterial.size() != ShadowConstantStride)
                {
                    Log::Error("Cutout shadow constants are missing");
                    return false;
                }

                std::memcpy(Destination, Batch->ShadowMaterial.data(), ShadowConstantStride);
                std::memcpy(Destination, Draws[Index].ViewProjection, sizeof(Draws[Index].ViewProjection));
                std::memcpy(Destination + 64, Draws[Index].World, sizeof(Draws[Index].World));
                std::memcpy(Destination + 128, Draws[Index].NormalMatrix, sizeof(Draws[Index].NormalMatrix));
                std::memcpy(Destination + 192, Draws[Index].LightPosition, sizeof(Draws[Index].LightPosition));
            }
            else
            {
                std::memcpy(Destination, Draws[Index].ViewProjection, sizeof(Draws[Index].ViewProjection));
                std::memcpy(Destination + 64, Draws[Index].World, sizeof(Draws[Index].World));
                std::memcpy(Destination + 128, Draws[Index].LightPosition, sizeof(Draws[Index].LightPosition));
            }
        }
        if (!Draws.empty() && !ConstantBuffer->Update(0, Constants))
        {
            Log::Error("Failed to write shadow constants");
            return false;
        }

        RHITexture* OpenTarget = nullptr;
        for (size_t Index = 0; Index < Draws.size(); ++Index)
        {
            const ShadowDraw& Draw = Draws[Index];
            if (Draw.bBegin)
            {
                if (OpenTarget != nullptr && OpenTarget != Draw.Target)
                {
                    Commands.UseAsShaderResource(*OpenTarget);
                }
                Commands.BeginDepthPass(*Draw.Target, true, Draw.Face);
                Commands.SetViewport({0.0F, 0.0F, static_cast<float>(Draw.Size), static_cast<float>(Draw.Size), 0.0F, 1.0F});
                Commands.SetScissor({0, 0, static_cast<int32>(Draw.Size), static_cast<int32>(Draw.Size)});
                OpenTarget = Draw.Target;
            }

            if (Draw.Batch == nullptr || Draw.Pipeline == nullptr)
            {
                continue;
            }

            Commands.SetPipeline(*Draw.Pipeline);
            Commands.BindShaderResources();
            Commands.SetVertexBuffer(*Draw.Batch->VertexBuffer);
            Commands.SetIndexBuffer(*Draw.Batch->IndexBuffer);
            Commands.SetConstantBuffer(*ConstantBuffer, static_cast<uint32>(Index) * ShadowConstantStride);
            Commands.DrawIndexed(Draw.Batch->IndexCount);
        }
        if (OpenTarget != nullptr)
        {
            Commands.EndRenderPass();
            Commands.UseAsShaderResource(*OpenTarget);
        }
        return true;
    }
}
