#include "Framework/Render/Public/ForwardRenderPass.h"

#include "Framework/Render/Public/ForwardShadows.h"
#include "Framework/Render/Public/ImageBasedLighting.h"
#include "Framework/Render/Public/PostProcess.h"
#include "Framework/Render/Public/SkyBackground.h"

#include "Framework/Asset/Public/AssetManager.h"
#include "Framework/Asset/Public/MeshAsset.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/Material/Public/Material.h"
#include "Framework/Material/Public/MaterialInstance.h"
#include "Framework/Material/Public/MaterialParameter.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/RHI/Public/RHISwapChain.h"
#include "Framework/RHI/Public/RHIPipelineLayout.h"
#include "Framework/RHI/Public/RHITexture.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace ShadowEngine
{
    namespace
    {
        uint32 PointLightCapacity(const RHIPipelineLayout& Layout)
        {
            uint32 Capacity = 0;
            for (const RHIPipelineLayout::Uniform& Uniform : Layout.Uniforms)
            {
                if (Uniform.Name.rfind("ForwardPointLight[", 0) == 0 && Uniform.Name.ends_with(".Position"))
                {
                    ++Capacity;
                }
            }
            return Capacity;
        }

        bool WriteImageBasedLight(uint8* Destination, const RHIPipelineLayout& Layout, const ImageBasedLightBinding& Binding)
        {
            const auto Write = [&](const char* Name, uint32 Index) -> bool
            {
                if (Layout.WriteDescriptorIndex(Destination, Name, Index))
                {
                    return true;
                }

                Log::Error("Image based light binding {} is missing from the constant layout", Name);
                return false;
            };

            return Write("Irradiance", Binding.Irradiance) &&
                Write("Prefiltered0", Binding.Prefiltered0) &&
                Write("Prefiltered1", Binding.Prefiltered1) &&
                Write("Prefiltered2", Binding.Prefiltered2) &&
                Write("Prefiltered3", Binding.Prefiltered3) &&
                Write("BrdfLut", Binding.BrdfLut) &&
                Write("EnvironmentSampler", Binding.EnvironmentSampler) &&
                Write("BrdfSampler", Binding.BrdfSampler);
        }

        bool WriteShadowUniform(uint8* Destination, const RHIPipelineLayout& Layout, const char* Name, const float Value[4])
        {
            for (const RHIPipelineLayout::Uniform& Uniform : Layout.Uniforms)
            {
                if (Uniform.Name == Name)
                {
                    Layout.WriteUniform(Destination, Name, Value);
                    return true;
                }
            }

            Log::Error("Shadow binding {} is missing from the constant layout", Name);
            return false;
        }

        bool WriteShadow(uint8* Destination, const RHIPipelineLayout& Layout, const ForwardShadowBinding& Binding)
        {
            const float Settings[4] = {Binding.Settings[0], Binding.Settings[1], Binding.Settings[2], Binding.Settings[3]};
            if (!WriteShadowUniform(Destination, Layout, "ForwardShadow.DirectRow0", Binding.DirectRow[0]) ||
                !WriteShadowUniform(Destination, Layout, "ForwardShadow.DirectRow1", Binding.DirectRow[1]) ||
                !WriteShadowUniform(Destination, Layout, "ForwardShadow.DirectRow2", Binding.DirectRow[2]) ||
                !WriteShadowUniform(Destination, Layout, "ForwardShadow.DirectRow3", Binding.DirectRow[3]) ||
                !WriteShadowUniform(Destination, Layout, "ForwardShadow.Settings", Settings))
            {
                return false;
            }

            const auto Write = [&](const char* Name, uint32 Index) -> bool
            {
                if (Layout.WriteDescriptorIndex(Destination, Name, Index))
                {
                    return true;
                }

                Log::Error("Shadow binding {} is missing from the constant layout", Name);
                return false;
            };

            return Write("DirectMap", Binding.DirectMap) &&
                Write("PointMap0", Binding.PointMap[0]) &&
                Write("PointMap1", Binding.PointMap[1]) &&
                Write("PointMap2", Binding.PointMap[2]) &&
                Write("PointMap3", Binding.PointMap[3]) &&
                Write("ComparisonSampler", Binding.ComparisonSampler);
        }

        struct GpuObjectConstants
        {
            float ViewProjection[16];
            float World[16];
            float NormalMatrix[16];
            float CameraPosition[4];
            float LightDirection[4];
            float LightColor[4];
            float PreviousViewProjection[16];
            float PreviousWorld[16];
        };
    }

    bool ForwardRenderPass::Execute(const RenderQueue& Queue, const RenderPassContext& Context)
    {
        static_assert(sizeof(GpuObjectConstants) == 368);
        static_assert(sizeof(GpuObjectConstants) <= 1024);
        if (Context.Device == nullptr || Context.SwapChain == nullptr || Context.CommandList == nullptr ||
            Context.ConstantBuffer == nullptr || Context.DepthBuffer == nullptr || Context.Assets == nullptr ||
            Context.Shaders == nullptr || Context.View == nullptr || Context.Batches == nullptr ||
            Context.TextureDescriptors == nullptr || Context.Samplers == nullptr ||
            Context.PointLights == nullptr || Context.SceneColor == nullptr ||
            Context.DepthPrepassPipeline == nullptr || Context.DepthPrepassTwoSidedPipeline == nullptr ||
            Context.ImageBasedLight == nullptr || Context.Post == nullptr ||
            Context.Shadow == nullptr || Context.Shadows == nullptr || Context.ShadowCasters == nullptr)
        {
            return false;
        }

        const std::vector<RenderItem>& Items = Queue.GetItems();
        if (Items.size() > Context.ConstantCapacity)
        {
            Log::Error("Scene has more objects than the constant buffer can hold");
            return false;
        }

        const float Width = static_cast<float>(Context.SwapChain->GetWidth());
        const float Height = static_cast<float>(Context.SwapChain->GetHeight());
        const float Aspect = Height > 0.0F ? Width / Height : 1.0F;
        const glm::mat4 View = Context.View->ViewMatrix();
        glm::mat4 Projection = glm::perspectiveRH_ZO(glm::radians(45.0F), Aspect, 0.1F, 100.0F);
        ApplyProjectionJitter(Projection, Context.ProjectionJitterX, Context.ProjectionJitterY, Width, Height);
        const glm::mat4 ViewProjection = Projection * View;
        const glm::mat4 InverseViewProjection = glm::inverse(ViewProjection);
        const glm::vec3 CameraPosition = Context.View->GetPosition();
        if (Context.Post == nullptr || Context.SceneColor == nullptr)
        {
            Log::Error("Forward post process is missing");
            return false;
        }

        DirectLightRenderProxy Direct{};
        Direct.Intensity = 0.0F;
        if (Context.DirectLight != nullptr)
        {
            Direct = *Context.DirectLight;
        }

        SkyLightRenderProxy Sky{};
        Sky.Intensity = 0.0F;
        if (Context.SkyLight != nullptr)
        {
            Sky = *Context.SkyLight;
        }

        std::vector<uint8> Constants(Items.size() * Context.ConstantAlignment);
        for (size_t Index = 0; Index < Items.size(); ++Index)
        {
            const RenderItem& Item = Items[Index];
            const MeshAsset* Mesh = Context.Assets->ResolveMesh(Item.Mesh);
            const MaterialInstance* Instance = Context.Assets->ResolveMaterialInstance(Item.Material);
            const Material* BoundMaterial = Instance == nullptr ? nullptr : Context.Assets->ResolveMaterial(Instance->GetBaseMaterial());
            const ShaderHandle* Shader = BoundMaterial == nullptr ? nullptr : BoundMaterial->FindShader("ForwardRenderer");
            if (Mesh == nullptr || Instance == nullptr || Shader == nullptr ||
                Context.Shaders->GetVertexShader(*Shader) == nullptr ||
                Context.Shaders->GetPixelShader(*Shader) == nullptr ||
                FindMeshBatch(*Context.Batches, Item.Mesh, Item.Section) == nullptr)
            {
                Log::Error("Render item is missing a mesh, material, or shader");
                return false;
            }

            const MaterialParameterBlock* Parameter = Instance->FindParameters("ForwardRenderer");
            GpuObjectConstants Slot{};
            std::memcpy(Slot.ViewProjection, glm::value_ptr(ViewProjection), sizeof(Slot.ViewProjection));
            std::memcpy(Slot.World, Item.World, sizeof(Slot.World));
            std::memcpy(Slot.NormalMatrix, Item.NormalMatrix, sizeof(Slot.NormalMatrix));
            Slot.CameraPosition[0] = CameraPosition.x;
            Slot.CameraPosition[1] = CameraPosition.y;
            Slot.CameraPosition[2] = CameraPosition.z;
            Slot.LightDirection[0] = Direct.Direction[0];
            Slot.LightDirection[1] = Direct.Direction[1];
            Slot.LightDirection[2] = Direct.Direction[2];
            Slot.LightColor[0] = Direct.Color[0];
            Slot.LightColor[1] = Direct.Color[1];
            Slot.LightColor[2] = Direct.Color[2];
            Slot.LightColor[3] = Direct.Intensity;
            if (Context.PreviousViewProjection != nullptr)
            {
                std::memcpy(Slot.PreviousViewProjection, Context.PreviousViewProjection, sizeof(Slot.PreviousViewProjection));
            }
            else
            {
                std::memcpy(Slot.PreviousViewProjection, Slot.ViewProjection, sizeof(Slot.PreviousViewProjection));
            }
            const size_t PreviousIndex = static_cast<size_t>(Items[Index].ProxyIndex) * 16;
            if (Context.PreviousWorlds != nullptr && PreviousIndex + 16 <= Context.PreviousWorlds->size())
            {
                std::memcpy(Slot.PreviousWorld, Context.PreviousWorlds->data() + PreviousIndex, sizeof(Slot.PreviousWorld));
            }
            else
            {
                std::memcpy(Slot.PreviousWorld, Slot.World, sizeof(Slot.PreviousWorld));
            }
            const float DirectDirection[4] = {Direct.Direction[0], Direct.Direction[1], Direct.Direction[2], 0.0F};
            const float DirectColor[4] = {Direct.Color[0], Direct.Color[1], Direct.Color[2], Direct.Intensity};
            const float SkyColor[4] = {Sky.Color[0], Sky.Color[1], Sky.Color[2], Sky.Intensity};
            MaterialParameter::TextureBinding Textures;
            std::map<std::string, std::array<float, 4>> ScalarValues;
            if (Parameter != nullptr)
            {
                Textures = MaterialParameter::GetTextureBinding(*Parameter);
                for (const auto& [Name, Scalar] : Parameter->Scalars)
                {
                    ScalarValues.emplace(Name, Scalar.Value);
                }
            }
            const auto ResolveTexture = [&](TextureAssetHandle Handle, uint32& Index) -> bool
            {
                if (!Handle.IsValid())
                {
                    return true;
                }

                const auto Found = Context.TextureDescriptors->find(Handle.Index);
                if (Found == Context.TextureDescriptors->end())
                {
                    Log::Error("Material descriptor is missing");
                    return false;
                }

                Index = Found->second;
                return true;
            };
            uint32 SamplerDescriptor = InvalidShaderDescriptor;
            std::map<std::string, uint32> TextureIndices;
            for (const auto& [Name, Handle] : Textures.Textures)
            {
                uint32 Index = InvalidShaderDescriptor;
                if (!ResolveTexture(Handle, Index))
                {
                    return false;
                }

                TextureIndices.emplace(Name, Index);
            }
            if (Textures.Sampler.IsValid())
            {
                const auto Sampler = Context.Samplers->find(Textures.Sampler.Index);
                if (Sampler == Context.Samplers->end() || Sampler->second == nullptr)
                {
                    Log::Error("Material descriptor is missing");
                    return false;
                }

                SamplerDescriptor = Sampler->second->GetDescriptorIndex();
            }

            uint8* Destination = Constants.data() + Index * Context.ConstantAlignment;
            std::memcpy(Destination, &Slot, sizeof(Slot));
            const MeshBatch* Batch = FindMeshBatch(*Context.Batches, Item.Mesh, Item.Section);
            if (Batch != nullptr)
            {
                Batch->Layout.WriteScalars(Destination, ScalarValues);
                Batch->Layout.WriteUniform(Destination, "ForwardDirectLight.Direction", DirectDirection);
                uint32 DirectOffset = 0xffffffff;
                for (const RHIPipelineLayout::Uniform& Uniform : Batch->Layout.Uniforms)
                {
                    if (Uniform.Name == "ForwardDirectLight.Direction")
                    {
                        DirectOffset = Uniform.Offset;
                        break;
                    }
                }
                if (DirectOffset < sizeof(Slot))
                {
                    Log::Error("Forward previous matrices are missing from the constant layout");
                    return false;
                }
                Batch->Layout.WriteUniform(Destination, "ForwardDirectLight.Color", DirectColor);
                const uint32 PointCapacity = PointLightCapacity(Batch->Layout);
                if (PointCapacity == 0 && !Context.PointLights->empty())
                {
                    Log::Error("Forward point light uniforms are missing");
                    return false;
                }
                static size_t LoggedPointLightCount = 0;
                if (Context.PointLights->size() > PointCapacity && LoggedPointLightCount != Context.PointLights->size())
                {
                    Log::Warning(
                        "Forward renderer keeps the first {} point lights and ignores {}",
                        PointCapacity,
                        Context.PointLights->size() - PointCapacity);
                    LoggedPointLightCount = Context.PointLights->size();
                }
                const size_t PointCount = std::min(Context.PointLights->size(), static_cast<size_t>(PointCapacity));
                for (size_t LightIndex = 0; LightIndex < PointCount; ++LightIndex)
                {
                    const PointLightRenderProxy& Point = (*Context.PointLights)[LightIndex];
                    const float PointPosition[4] = {Point.Position[0], Point.Position[1], Point.Position[2], Point.Radius};
                    const float PointColor[4] = {Point.Color[0], Point.Color[1], Point.Color[2], Point.Intensity};
                    const std::string Prefix = "ForwardPointLight[" + std::to_string(LightIndex) + "]";
                    Batch->Layout.WriteUniform(Destination, Prefix + ".Position", PointPosition);
                    Batch->Layout.WriteUniform(Destination, Prefix + ".Color", PointColor);
                }
                Batch->Layout.WriteUniform(Destination, "ForwardSkyLight.Color", SkyColor);
                Batch->Layout.WriteDescriptors(Destination, TextureIndices, SamplerDescriptor);
                ForwardShadowBinding ShadowBinding = *Context.Shadow;
                if (Batch->Surface == EMaterialSurface::Translucent)
                {
                    ShadowBinding.DirectMap = InvalidShaderDescriptor;
                    for (uint32& PointMap : ShadowBinding.PointMap)
                    {
                        PointMap = InvalidShaderDescriptor;
                    }
                }
                if (!WriteImageBasedLight(Destination, Batch->Layout, *Context.ImageBasedLight) ||
                    !WriteShadow(Destination, Batch->Layout, ShadowBinding))
                {
                    return false;
                }
            }
        }
        if (!Constants.empty() && !Context.ConstantBuffer->Update(0, Constants))
        {
            Log::Error("Failed to write scene constants");
            return false;
        }

        Context.CommandList->Begin();
        if (!Context.Shadows->Record(*Context.CommandList, *Context.Batches, *Context.ShadowCasters))
        {
            Context.CommandList->End();
            return false;
        }
        const RHIRect Window{
            0,
            0,
            static_cast<int32>(Context.SwapChain->GetWidth()),
            static_cast<int32>(Context.SwapChain->GetHeight())};
        std::vector<uint32> DepthItems;
        std::vector<uint32> TranslucentItems;
        DepthItems.reserve(Items.size());
        TranslucentItems.reserve(Items.size());
        for (uint32 Index = 0; Index < static_cast<uint32>(Items.size()); ++Index)
        {
            const MeshBatch* Batch = FindMeshBatch(*Context.Batches, Items[Index].Mesh, Items[Index].Section);
            if (Batch->Surface == EMaterialSurface::Translucent)
            {
                TranslucentItems.push_back(Index);
            }
            else
            {
                DepthItems.push_back(Index);
            }
        }
        const auto SurfaceDistance = [&](uint32 Index) -> float
        {
            const float* World = Items[Index].World;
            const float OffsetX = World[12] - CameraPosition.x;
            const float OffsetY = World[13] - CameraPosition.y;
            const float OffsetZ = World[14] - CameraPosition.z;
            return OffsetX * OffsetX + OffsetY * OffsetY + OffsetZ * OffsetZ;
        };
        std::stable_sort(TranslucentItems.begin(), TranslucentItems.end(), [&](uint32 Left, uint32 Right)
        {
            return SurfaceDistance(Left) > SurfaceDistance(Right);
        });
        const auto DrawShaded = [&](uint32 Index)
        {
            const MeshBatch* Batch = FindMeshBatch(*Context.Batches, Items[Index].Mesh, Items[Index].Section);
            Context.CommandList->SetPipeline(*Batch->Pipeline);
            Context.CommandList->BindShaderResources();
            Context.CommandList->SetVertexBuffer(*Batch->VertexBuffer);
            Context.CommandList->SetIndexBuffer(*Batch->IndexBuffer);
            Context.CommandList->SetConstantBuffer(*Context.ConstantBuffer, Index * Context.ConstantAlignment);
            Context.CommandList->DrawIndexed(Batch->IndexCount);
        };

        Context.CommandList->BeginRenderPass(*Context.SceneColor, Context.DepthBuffer, Context.ClearColor, false, true);
        Context.CommandList->SetViewport({0.0F, 0.0F, Width, Height, 0.0F, 1.0F});
        Context.CommandList->SetScissor(Window);
        for (const uint32 Index : DepthItems)
        {
            const MeshBatch* Batch = FindMeshBatch(*Context.Batches, Items[Index].Mesh, Items[Index].Section);
            RHIPipeline* DepthPipeline = Batch->bTwoSided ? Context.DepthPrepassTwoSidedPipeline : Context.DepthPrepassPipeline;
            if (Batch->Surface == EMaterialSurface::Cutout)
            {
                if (Batch->DepthPipeline == nullptr)
                {
                    Log::Error("Cutout material is missing its depth pipeline");
                    Context.CommandList->EndRenderPass();
                    Context.CommandList->End();
                    return false;
                }

                DepthPipeline = Batch->DepthPipeline.get();
            }

            Context.CommandList->SetPipeline(*DepthPipeline);
            Context.CommandList->BindShaderResources();
            Context.CommandList->SetVertexBuffer(*Batch->VertexBuffer);
            Context.CommandList->SetIndexBuffer(*Batch->IndexBuffer);
            Context.CommandList->SetConstantBuffer(*Context.ConstantBuffer, Index * Context.ConstantAlignment);
            Context.CommandList->DrawIndexed(Batch->IndexCount);
        }
        Context.CommandList->EndRenderPass();

        Context.CommandList->BeginRenderPass(*Context.SceneColor, Context.DepthBuffer, Context.ClearColor, true, false);
        Context.CommandList->SetViewport({0.0F, 0.0F, Width, Height, 0.0F, 1.0F});
        Context.CommandList->SetScissor(Window);
        if (!DrawSkyBackground(Context))
        {
            Context.CommandList->End();
            return false;
        }
        for (const uint32 Index : DepthItems)
        {
            DrawShaded(Index);
        }
        for (const uint32 Index : TranslucentItems)
        {
            DrawShaded(Index);
        }
        Context.CommandList->EndRenderPass();

        float InverseViewProjectionValues[16];
        std::memcpy(InverseViewProjectionValues, glm::value_ptr(InverseViewProjection), sizeof(InverseViewProjectionValues));
        float ViewProjectionValues[16];
        std::memcpy(ViewProjectionValues, glm::value_ptr(ViewProjection), sizeof(ViewProjectionValues));
        const float* PreviousViewProjection = Context.PreviousViewProjection != nullptr ? Context.PreviousViewProjection : ViewProjectionValues;
        const float CameraPositionValues[3] = {CameraPosition.x, CameraPosition.y, CameraPosition.z};
        if (!Context.Post->Draw(
                *Context.CommandList,
                Context.SwapChain->GetCurrentBackBuffer(),
                *Context.DepthBuffer,
                *Context.ConstantBuffer,
                Items,
                *Context.Batches,
                InverseViewProjectionValues,
                PreviousViewProjection,
                CameraPositionValues,
                Context.SwapChain->GetWidth(),
                Context.SwapChain->GetHeight(),
                Context.ConstantAlignment,
                Context.FrameDeltaTime))
        {
            Context.CommandList->End();
            return false;
        }

        if (Context.StoredViewProjection != nullptr)
        {
            std::memcpy(Context.StoredViewProjection, ViewProjectionValues, sizeof(ViewProjectionValues));
        }
        Context.CommandList->End();

        Context.Device->SubmitCommandList(*Context.CommandList);
        const bool bPresented = Context.SwapChain->Present();
        Context.Device->WaitIdle();
        return bPresented;
    }
}
