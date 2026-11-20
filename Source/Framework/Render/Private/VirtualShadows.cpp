#include "ThirdPartyAdapter/Mathematics/Public/GLMAdapter.h"
#include "Framework/Render/Public/VirtualShadows.h"

#include "Framework/Asset/Public/AssetManager.h"
#include "Framework/Asset/Public/MeshAsset.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/UI/Public/UIManager.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIDefinitions.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "Framework/RHI/Public/RHISampler.h"
#include "Framework/RHI/Public/RHITexture.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>

namespace ShadowEngine
{
    namespace
    {
        constexpr uint32 PageSize = 128;
        constexpr uint32 PagesPerAxis = 8;
        constexpr uint32 ClipLevelCount = 4;
        constexpr uint32 PhysicalPagesX = 16;
        constexpr uint32 PhysicalPagesY = 16;
        constexpr uint32 PhysicalPageCount = PhysicalPagesX * PhysicalPagesY;
        constexpr uint32 AtlasWidth = PhysicalPagesX * PageSize;
        constexpr uint32 AtlasHeight = PhysicalPagesY * PageSize;
        constexpr uint32 PageTableWidth = PagesPerAxis;
        constexpr uint32 PageTableHeight = PagesPerAxis * ClipLevelCount;
        constexpr uint32 PointShadowFaceSize = 1024;
        constexpr uint32 ShadowConstantSlots = 4096;
        constexpr uint32 ShadowConstantStride = 1024;
        constexpr float LevelWorldSize[ClipLevelCount] = {8.0F, 16.0F, 32.0F, 64.0F};
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
            float ViewportX = 0.0F;
            float ViewportY = 0.0F;
            float ViewportW = 0.0F;
            float ViewportH = 0.0F;
            uint32 Face = 0;
            bool bBegin = false;
            bool bViewport = false;
            bool bClip = false;
        };

        struct ViewBox
        {
            const MeshRenderProxy* Caster = nullptr;
            const MeshBatch* Batch = nullptr;
            Vec3 Min = {};
            Vec3 Max = {};
        };

        Vec3 TransformPoint(const float World[16], float X, float Y, float Z)
        {
            return Vec3(GLMAdapter::MakeMat4(World) * Vec4(X, Y, Z, 1.0F));
        }

        void StoreRows(const Mat4& Clip, float OutRows[4][4])
        {
            for (int Row = 0; Row < 4; ++Row)
            {
                OutRows[Row][0] = Clip[0][Row];
                OutRows[Row][1] = Clip[1][Row];
                OutRows[Row][2] = Clip[2][Row];
                OutRows[Row][3] = Clip[3][Row];
            }
        }

        bool AppendBox(
            std::vector<ViewBox>& Boxes,
            const MeshBatch& Batch,
            const MeshRenderProxy& Caster,
            const MeshSection& Section,
            const Mat4& LightView)
        {
            if (Batch.Surface == EMaterialSurface::Translucent || Batch.VertexBuffer == nullptr || Batch.IndexBuffer == nullptr)
            {
                return true;
            }

            ViewBox Box;
            Box.Caster = &Caster;
            Box.Batch = &Batch;
            bool bAny = false;
            for (uint32 Corner = 0; Corner < 8; ++Corner)
            {
                const Vec3 WorldPoint = TransformPoint(
                    Caster.World,
                    (Corner & 1U) != 0U ? Section.BoundsMax[0] : Section.BoundsMin[0],
                    (Corner & 2U) != 0U ? Section.BoundsMax[1] : Section.BoundsMin[1],
                    (Corner & 4U) != 0U ? Section.BoundsMax[2] : Section.BoundsMin[2]);
                const Vec3 ViewPoint = Vec3(LightView * Vec4(WorldPoint, 1.0F));
                if (!bAny)
                {
                    Box.Min = ViewPoint;
                    Box.Max = ViewPoint;
                    bAny = true;
                }
                else
                {
                    Box.Min = GLMAdapter::Min(Box.Min, ViewPoint);
                    Box.Max = GLMAdapter::Max(Box.Max, ViewPoint);
                }
            }
            if (bAny)
            {
                Boxes.push_back(Box);
            }
            return true;
        }

        bool CollectBoxes(
            std::vector<ViewBox>& Boxes,
            const std::vector<MeshBatch>& Batches,
            const std::vector<MeshRenderProxy>& Casters,
            const AssetManager& Assets,
            const Mat4& LightView)
        {
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
                    const MeshBatch* Batch = FindMeshBatch(Batches, Caster.Mesh, Section);
                    if (Batch == nullptr)
                    {
                        continue;
                    }
                    if (!AppendBox(Boxes, *Batch, Caster, Sections[Section], LightView))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        void AddDraw(
            std::vector<ShadowDraw>& Draws,
            const ViewBox* Box,
            const float ViewProjection[16],
            const float LightPosition[4],
            RHITexture& Target,
            RHIPipeline& Pipeline,
            float ViewportX,
            float ViewportY,
            float ViewportW,
            float ViewportH,
            uint32 Face,
            bool bBegin,
            bool bViewport,
            bool bPoint)
        {
            ShadowDraw Draw;
            if (Box != nullptr)
            {
                Draw.Batch = Box->Batch;
                std::memcpy(Draw.World, Box->Caster->World, sizeof(Draw.World));
                std::memcpy(Draw.NormalMatrix, Box->Caster->NormalMatrix, sizeof(Draw.NormalMatrix));
                Draw.bClip = Box->Batch->Surface == EMaterialSurface::Cutout;
                if (Draw.bClip)
                {
                    RHIPipeline* ClipPipeline = bPoint ? Box->Batch->PointShadowPipeline.get() : Box->Batch->DirectionalShadowPipeline.get();
                    Draw.Pipeline = ClipPipeline == nullptr ? &Pipeline : ClipPipeline;
                }
            }
            if (Draw.Pipeline == nullptr)
            {
                Draw.Pipeline = &Pipeline;
            }
            if (ViewProjection != nullptr)
            {
                std::memcpy(Draw.ViewProjection, ViewProjection, sizeof(Draw.ViewProjection));
            }
            if (LightPosition != nullptr)
            {
                std::memcpy(Draw.LightPosition, LightPosition, sizeof(Draw.LightPosition));
            }
            Draw.Target = &Target;
            Draw.ViewportX = ViewportX;
            Draw.ViewportY = ViewportY;
            Draw.ViewportW = ViewportW;
            Draw.ViewportH = ViewportH;
            Draw.Face = Face;
            Draw.bBegin = bBegin;
            Draw.bViewport = bViewport;
            Draws.push_back(Draw);
        }
    }

    VirtualShadows::VirtualShadows() = default;
    VirtualShadows::~VirtualShadows() = default;

    std::unique_ptr<RHIPipeline> VirtualShadows::CreateDepthPipeline(
        RHIDevice& Device,
        ShaderManager& Shaders,
        const char* ShaderPath,
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
        PipelineDesc.RenderTargetCount = 0;
        PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
        PipelineDesc.InputLayout = InputLayout;
        PipelineDesc.bEnableDepth = true;
        PipelineDesc.bWriteDepth = true;
        PipelineDesc.bWriteColor = false;
        PipelineDesc.DepthFormat = ERHIFormat::D32_Float;
        PipelineDesc.Cull = Cull;
        PipelineDesc.bFrontCounterClockwise = true;
        return Device.CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
    }

    bool VirtualShadows::Ensure(RHIDevice& Device, ShaderManager& Shaders, std::string* ErrorMessage)
    {
        static_assert(sizeof(GpuShadowConstants) == 144);
        static_assert(sizeof(GpuShadowConstants) <= ShadowConstantStride);
        if (bReady)
        {
            return true;
        }

        DirectionalPipeline = CreateDepthPipeline(Device, Shaders, "Depth/DirectionalShadow.slang", ERHICullMode::Back, ErrorMessage);
        DirectionalTwoSidedPipeline = CreateDepthPipeline(Device, Shaders, "Depth/DirectionalShadow.slang", ERHICullMode::None, ErrorMessage);
        PointPipeline = CreateDepthPipeline(Device, Shaders, "Depth/PointShadow.slang", ERHICullMode::Back, ErrorMessage);
        PointTwoSidedPipeline = CreateDepthPipeline(Device, Shaders, "Depth/PointShadow.slang", ERHICullMode::None, ErrorMessage);
        if (DirectionalPipeline == nullptr || DirectionalTwoSidedPipeline == nullptr || PointPipeline == nullptr ||
            PointTwoSidedPipeline == nullptr)
        {
            return false;
        }

        Atlas = Device.CreateDepthTexture(AtlasWidth, AtlasHeight, ErrorMessage);
        if (Atlas == nullptr || !Device.CreateShaderResourceView(*Atlas, AtlasDescriptor, ErrorMessage))
        {
            return false;
        }

        PagePixels.assign(static_cast<size_t>(PageTableWidth) * PageTableHeight * 4, 0);
        RHITextureMipDesc Mip;
        Mip.Width = PageTableWidth;
        Mip.Height = PageTableHeight;
        Mip.Pixels = PagePixels.data();
        Mip.Size = static_cast<uint32>(PagePixels.size());
        RHITextureDesc PageDesc;
        PageDesc.Format = ERHIFormat::R8G8B8A8_UNorm;
        PageDesc.Mips = std::span<const RHITextureMipDesc>(&Mip, 1);
        PageTable = Device.CreateTexture(PageDesc, ErrorMessage);
        if (PageTable == nullptr || !Device.CreateShaderResourceView(*PageTable, PageDescriptor, ErrorMessage))
        {
            return false;
        }

        for (uint32 Index = 0; Index < 4; ++Index)
        {
            PointMaps[Index] = Device.CreateCubeDepth(PointShadowFaceSize, ErrorMessage);
            if (PointMaps[Index] == nullptr || !Device.CreateShaderResourceView(*PointMaps[Index], PointDescriptors[Index], ErrorMessage))
            {
                return false;
            }
        }

        ComparisonSampler = Device.CreateComparisonSampler(ErrorMessage);
        PageSampler = Device.CreatePointClampSampler(ErrorMessage);
        ConstantBuffer = Device.CreateConstantBuffer(ShadowConstantSlots * ShadowConstantStride, ErrorMessage);
        if (ComparisonSampler == nullptr || PageSampler == nullptr || ConstantBuffer == nullptr)
        {
            return false;
        }

        ComparisonDescriptor = ComparisonSampler->GetDescriptorIndex();
        PageSamplerDescriptor = PageSampler->GetDescriptorIndex();
        bReady = true;
        Log::Info(
            "Virtual shadows ready: {} clip levels, page {}, atlas {}x{}, point cube {}",
            ClipLevelCount,
            PageSize,
            AtlasWidth,
            AtlasHeight,
            PointShadowFaceSize);
        return true;
    }

    bool VirtualShadows::Prepare(
        const std::vector<MeshBatch>& Batches,
        const std::vector<MeshRenderProxy>& Casters,
        const AssetManager& Assets,
        const Camera& View,
        const DirectLightRenderProxy* DirectLight,
        const std::vector<PointLightRenderProxy>& PointLights,
        std::string* ErrorMessage)
    {
        if (!bReady)
        {
            SetErrorMessage(ErrorMessage, "Virtual shadow maps are not ready");
            return false;
        }

        bDirectActive = false;
        for (bool& Active : bPointActive)
        {
            Active = false;
        }
        Pages.clear();
        CasterBounds.clear();
        std::fill(PagePixels.begin(), PagePixels.end(), static_cast<uint8>(0));
        for (ClipLevel& Level : Levels)
        {
            Level = {};
        }

        const UIManager::LightState LightSettings = UIManager::Get().GetState().Light;
        Binding = {};
        Binding.ComparisonSampler = ComparisonDescriptor;
        Binding.Settings[0] = 1.0F / static_cast<float>(PageSize * PagesPerAxis);
        Binding.Settings[1] = LightSettings.ShadowNormalBias;
        Binding.Settings[2] = 2.0F / static_cast<float>(PointShadowFaceSize);
        Binding.Settings[3] = LightSettings.PointShadowDepthBias;
        Binding.VirtualInfo[0] = static_cast<float>(PagesPerAxis);
        Binding.VirtualInfo[1] = static_cast<float>(PageSize);
        Binding.VirtualInfo[2] = static_cast<float>(PhysicalPagesX);
        Binding.VirtualInfo[3] = static_cast<float>(PhysicalPagesY);

        const size_t PointCount = LightSettings.bPointShadow
            ? std::min(PointLights.size(), static_cast<size_t>(4))
            : 0;
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

        const Vec3 RawDirection = DirectLight == nullptr
            ? Vec3(0.0F)
            : Vec3(DirectLight->Direction[0], DirectLight->Direction[1], DirectLight->Direction[2]);
        if (!LightSettings.bDirectionalShadow || DirectLight == nullptr || DirectLight->Intensity <= 0.0F ||
            GLMAdapter::Length(RawDirection) < 0.0001F)
        {
            return true;
        }

        const Vec3 Direction = GLMAdapter::Normalize(RawDirection);
        const Vec3 Up = std::abs(Direction.y) > 0.99F ? Vec3(0.0F, 0.0F, 1.0F) : Vec3(0.0F, 1.0F, 0.0F);
        const Vec3 CameraPosition = View.GetPosition();
        const Mat4 ViewMatrix = GLMAdapter::LookAtRH(CameraPosition - Direction * 80.0F, CameraPosition, Up);
        std::memcpy(LightView, GLMAdapter::ValuePtr(ViewMatrix), sizeof(LightView));

        std::vector<ViewBox> Boxes;
        if (!CollectBoxes(Boxes, Batches, Casters, Assets, ViewMatrix))
        {
            return false;
        }
        CasterBounds.reserve(Boxes.size());
        for (const ViewBox& Box : Boxes)
        {
            CasterBound Bound;
            Bound.Batch = Box.Batch;
            std::memcpy(Bound.World, Box.Caster->World, sizeof(Bound.World));
            std::memcpy(Bound.NormalMatrix, Box.Caster->NormalMatrix, sizeof(Bound.NormalMatrix));
            Bound.MinX = Box.Min.x;
            Bound.MinY = Box.Min.y;
            Bound.MaxX = Box.Max.x;
            Bound.MaxY = Box.Max.y;
            Bound.bTwoSided = Box.Batch->bTwoSided;
            CasterBounds.push_back(Bound);
        }

        const Vec3 CameraView = Vec3(ViewMatrix * Vec4(CameraPosition, 1.0F));
        uint32 Physical = 0;
        for (uint32 LevelIndex = 0; LevelIndex < ClipLevelCount; ++LevelIndex)
        {
            ClipLevel& Level = Levels[LevelIndex];
            const float Size = LevelWorldSize[LevelIndex];
            const float PageWorld = Size / static_cast<float>(PagesPerAxis);
            const float CenterX = std::floor(CameraView.x / PageWorld) * PageWorld;
            const float CenterY = std::floor(CameraView.y / PageWorld) * PageWorld;
            Level.Size = Size;
            Level.MinX = CenterX - Size * 0.5F;
            Level.MinY = CenterY - Size * 0.5F;

            bool bDepth = false;
            float DepthMin = 0.0F;
            float DepthMax = 0.0F;
            bool Marked[PagesPerAxis][PagesPerAxis] = {};
            for (const ViewBox& Box : Boxes)
            {
                const float OverMinX = std::max(Box.Min.x, Level.MinX);
                const float OverMaxX = std::min(Box.Max.x, Level.MinX + Size);
                const float OverMinY = std::max(Box.Min.y, Level.MinY);
                const float OverMaxY = std::min(Box.Max.y, Level.MinY + Size);
                if (OverMinX >= OverMaxX || OverMinY >= OverMaxY)
                {
                    continue;
                }

                if (!bDepth)
                {
                    DepthMin = Box.Min.z;
                    DepthMax = Box.Max.z;
                    bDepth = true;
                }
                else
                {
                    DepthMin = std::min(DepthMin, Box.Min.z);
                    DepthMax = std::max(DepthMax, Box.Max.z);
                }

                const auto PageOf = [&](float X, float Y, int32& OutX, int32& OutY)
                {
                    const float NdcX = ((X - Level.MinX) / Size) * 2.0F - 1.0F;
                    const float NdcY = ((Y - Level.MinY) / Size) * 2.0F - 1.0F;
                    const float UvX = NdcX * 0.5F + 0.5F;
                    const float UvY = -NdcY * 0.5F + 0.5F;
                    OutX = static_cast<int32>(std::floor(UvX * static_cast<float>(PagesPerAxis)));
                    OutY = static_cast<int32>(std::floor(UvY * static_cast<float>(PagesPerAxis)));
                };
                int32 X0 = 0;
                int32 Y0 = 0;
                int32 X1 = 0;
                int32 Y1 = 0;
                PageOf(OverMinX, OverMaxY, X0, Y0);
                PageOf(std::nextafter(OverMaxX, OverMinX), std::nextafter(OverMinY, OverMaxY), X1, Y1);
                const int32 PageMinX = std::clamp(std::min(X0, X1), 0, static_cast<int32>(PagesPerAxis - 1));
                const int32 PageMaxX = std::clamp(std::max(X0, X1), 0, static_cast<int32>(PagesPerAxis - 1));
                const int32 PageMinY = std::clamp(std::min(Y0, Y1), 0, static_cast<int32>(PagesPerAxis - 1));
                const int32 PageMaxY = std::clamp(std::max(Y0, Y1), 0, static_cast<int32>(PagesPerAxis - 1));
                for (int32 PageY = PageMinY; PageY <= PageMaxY; ++PageY)
                {
                    for (int32 PageX = PageMinX; PageX <= PageMaxX; ++PageX)
                    {
                        Marked[PageX][PageY] = true;
                    }
                }
            }

            Level.Near = bDepth ? std::max(0.05F, -DepthMax - 1.0F) : 0.05F;
            Level.Far = bDepth ? -DepthMin + 1.0F : 100.0F;
            if (Level.Far <= Level.Near)
            {
                Level.Far = Level.Near + 1.0F;
            }

            const Mat4 Clip = GLMAdapter::OrthoRH_ZO(
                Level.MinX,
                Level.MinX + Size,
                Level.MinY,
                Level.MinY + Size,
                Level.Near,
                Level.Far) * ViewMatrix;
            std::memcpy(Level.Clip, GLMAdapter::ValuePtr(Clip), sizeof(Level.Clip));
            StoreRows(Clip, Binding.LevelRow[LevelIndex]);
            Level.bActive = true;

            for (uint32 PageY = 0; PageY < PagesPerAxis; ++PageY)
            {
                for (uint32 PageX = 0; PageX < PagesPerAxis; ++PageX)
                {
                    if (!Marked[PageX][PageY] || Physical >= PhysicalPageCount)
                    {
                        continue;
                    }

                    VirtualPage Page;
                    Page.Level = LevelIndex;
                    Page.X = PageX;
                    Page.Y = PageY;
                    Page.Physical = Physical;
                    Page.MinX = Level.MinX + static_cast<float>(PageX) * PageWorld;
                    Page.MaxX = Page.MinX + PageWorld;
                    Page.MaxY = Level.MinY + Size - static_cast<float>(PageY) * PageWorld;
                    Page.MinY = Page.MaxY - PageWorld;
                    Pages.push_back(Page);

                    const uint32 PhysX = Physical % PhysicalPagesX;
                    const uint32 PhysY = Physical / PhysicalPagesX;
                    const uint32 Pixel = ((LevelIndex * PagesPerAxis + PageY) * PageTableWidth + PageX) * 4;
                    PagePixels[Pixel] = static_cast<uint8>(PhysX);
                    PagePixels[Pixel + 1] = static_cast<uint8>(PhysY);
                    PagePixels[Pixel + 2] = 255;
                    PagePixels[Pixel + 3] = 255;
                    ++Physical;
                }
            }
        }

        if (!Pages.empty())
        {
            const float WorldTexel = LevelWorldSize[0] / static_cast<float>(PagesPerAxis * PageSize);
            Binding.Settings[1] = std::max(WorldTexel * 3.0F, LightSettings.ShadowNormalBias);
            StoreRows(GLMAdapter::MakeMat4(Levels[0].Clip), Binding.DirectRow);
            Binding.DirectMap = AtlasDescriptor;
            Binding.PageTable = PageDescriptor;
            Binding.PageSampler = PageSamplerDescriptor;
            bDirectActive = true;
        }
        return true;
    }

    bool VirtualShadows::Record(
        RHICommandList& Commands,
        const std::vector<MeshBatch>& Batches,
        const std::vector<MeshRenderProxy>& Casters)
    {
        if (!bReady || ConstantBuffer == nullptr || PageTable == nullptr)
        {
            Log::Error("Virtual shadow maps are not ready");
            return false;
        }

        if (!Commands.UploadTexture(*PageTable, PagePixels))
        {
            Log::Error("Failed to upload the virtual shadow page table");
            return false;
        }

        std::vector<ShadowDraw> Draws;
        if (bDirectActive && Atlas != nullptr && DirectionalPipeline != nullptr && DirectionalTwoSidedPipeline != nullptr)
        {
            AddDraw(Draws, nullptr, nullptr, nullptr, *Atlas, *DirectionalPipeline, 0.0F, 0.0F, 0.0F, 0.0F, 0, true, false, false);
            const Mat4 ViewMatrix = GLMAdapter::MakeMat4(LightView);
            for (const VirtualPage& Page : Pages)
            {
                const Mat4 Clip = GLMAdapter::OrthoRH_ZO(
                    Page.MinX,
                    Page.MaxX,
                    Page.MinY,
                    Page.MaxY,
                    Levels[Page.Level].Near,
                    Levels[Page.Level].Far) * ViewMatrix;
                float ViewProjection[16];
                std::memcpy(ViewProjection, GLMAdapter::ValuePtr(Clip), sizeof(ViewProjection));
                const float OriginX = static_cast<float>((Page.Physical % PhysicalPagesX) * PageSize);
                const float OriginY = static_cast<float>((Page.Physical / PhysicalPagesX) * PageSize);
                bool bFirst = true;
                for (const CasterBound& Bound : CasterBounds)
                {
                    if (Bound.Batch == nullptr || Bound.MaxX < Page.MinX || Bound.MinX > Page.MaxX || Bound.MaxY < Page.MinY ||
                        Bound.MinY > Page.MaxY)
                    {
                        continue;
                    }

                    RHIPipeline* Pipeline = Bound.bTwoSided ? DirectionalTwoSidedPipeline.get() : DirectionalPipeline.get();
                    if (Bound.Batch->Surface == EMaterialSurface::Cutout)
                    {
                        Pipeline = Bound.Batch->DirectionalShadowPipeline.get();
                        if (Pipeline == nullptr)
                        {
                            Log::Error("Cutout shadow pipeline is missing");
                            return false;
                        }
                    }

                    ShadowDraw Draw;
                    Draw.Batch = Bound.Batch;
                    std::memcpy(Draw.ViewProjection, ViewProjection, sizeof(Draw.ViewProjection));
                    std::memcpy(Draw.World, Bound.World, sizeof(Draw.World));
                    std::memcpy(Draw.NormalMatrix, Bound.NormalMatrix, sizeof(Draw.NormalMatrix));
                    Draw.Target = Atlas.get();
                    Draw.Pipeline = Pipeline;
                    Draw.ViewportX = OriginX;
                    Draw.ViewportY = OriginY;
                    Draw.ViewportW = static_cast<float>(PageSize);
                    Draw.ViewportH = static_cast<float>(PageSize);
                    Draw.bBegin = false;
                    Draw.bViewport = bFirst;
                    Draw.bClip = Bound.Batch->Surface == EMaterialSurface::Cutout;
                    Draws.push_back(Draw);
                    bFirst = false;
                }
            }
        }

        static const Vec3 FaceDirection[6] = {
            {1.0F, 0.0F, 0.0F},
            {-1.0F, 0.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
            {0.0F, -1.0F, 0.0F},
            {0.0F, 0.0F, 1.0F},
            {0.0F, 0.0F, -1.0F},
        };
        static const Vec3 FaceUp[6] = {
            {0.0F, 1.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
            {0.0F, 0.0F, -1.0F},
            {0.0F, 0.0F, 1.0F},
            {0.0F, 1.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
        };
        for (uint32 LightIndex = 0; LightIndex < 4; ++LightIndex)
        {
            if (!bPointActive[LightIndex] || PointMaps[LightIndex] == nullptr || PointPipeline == nullptr || PointTwoSidedPipeline == nullptr)
            {
                continue;
            }

            const PointLightRenderProxy& Point = ActivePoints[LightIndex];
            const Vec3 Eye(Point.Position[0], Point.Position[1], Point.Position[2]);
            const float Far = std::max(Point.Radius, 0.1F);
            const Mat4 Projection = GLMAdapter::PerspectiveRH_ZO(GLMAdapter::Radians(90.0F), 1.0F, 0.05F, Far);
            const float LightPosition[4] = {Point.Position[0], Point.Position[1], Point.Position[2], Point.Radius};
            for (uint32 Face = 0; Face < 6; ++Face)
            {
                const Mat4 Clip = Projection * GLMAdapter::LookAtRH(Eye, Eye + FaceDirection[Face], FaceUp[Face]);
                float ViewProjection[16];
                std::memcpy(ViewProjection, GLMAdapter::ValuePtr(Clip), sizeof(ViewProjection));
                bool bFirst = true;
                bool bAny = false;
                for (const MeshRenderProxy& Caster : Casters)
                {
                    for (const MeshBatch& Batch : Batches)
                    {
                        if (Batch.Mesh.Index != Caster.Mesh.Index || Batch.Surface == EMaterialSurface::Translucent ||
                            Batch.VertexBuffer == nullptr || Batch.IndexBuffer == nullptr)
                        {
                            continue;
                        }

                        ViewBox Box;
                        Box.Caster = &Caster;
                        Box.Batch = &Batch;
                        RHIPipeline& Pipeline = Batch.bTwoSided ? *PointTwoSidedPipeline : *PointPipeline;
                        AddDraw(
                            Draws,
                            &Box,
                            ViewProjection,
                            LightPosition,
                            *PointMaps[LightIndex],
                            Pipeline,
                            0.0F,
                            0.0F,
                            static_cast<float>(PointShadowFaceSize),
                            static_cast<float>(PointShadowFaceSize),
                            Face,
                            bFirst,
                            bFirst,
                            true);
                        bFirst = false;
                        bAny = true;
                    }
                }
                if (!bAny)
                {
                    AddDraw(
                        Draws,
                        nullptr,
                        ViewProjection,
                        LightPosition,
                        *PointMaps[LightIndex],
                        *PointPipeline,
                        0.0F,
                        0.0F,
                        static_cast<float>(PointShadowFaceSize),
                        static_cast<float>(PointShadowFaceSize),
                        Face,
                        true,
                        true,
                        true);
                }
            }
        }

        if (Draws.size() > ShadowConstantSlots)
        {
            Log::Error("Shadow constant buffer is full");
            return false;
        }

        std::vector<uint8> Constants(Draws.size() * ShadowConstantStride);
        for (size_t Index = 0; Index < Draws.size(); ++Index)
        {
            uint8* Destination = Constants.data() + Index * ShadowConstantStride;
            const ShadowDraw& Draw = Draws[Index];
            if (Draw.Batch == nullptr)
            {
                continue;
            }
            if (Draw.bClip)
            {
                if (Draw.Batch->ShadowMaterial.size() != ShadowConstantStride)
                {
                    Log::Error("Cutout shadow constants are missing");
                    return false;
                }
                std::memcpy(Destination, Draw.Batch->ShadowMaterial.data(), ShadowConstantStride);
                std::memcpy(Destination, Draw.ViewProjection, sizeof(Draw.ViewProjection));
                std::memcpy(Destination + 64, Draw.World, sizeof(Draw.World));
                std::memcpy(Destination + 128, Draw.NormalMatrix, sizeof(Draw.NormalMatrix));
                std::memcpy(Destination + 192, Draw.LightPosition, sizeof(Draw.LightPosition));
            }
            else
            {
                std::memcpy(Destination, Draw.ViewProjection, sizeof(Draw.ViewProjection));
                std::memcpy(Destination + 64, Draw.World, sizeof(Draw.World));
                std::memcpy(Destination + 128, Draw.LightPosition, sizeof(Draw.LightPosition));
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
                OpenTarget = Draw.Target;
            }
            if (Draw.bViewport)
            {
                Commands.SetViewport({Draw.ViewportX, Draw.ViewportY, Draw.ViewportW, Draw.ViewportH, 0.0F, 1.0F});
                Commands.SetScissor({
                    static_cast<int32>(Draw.ViewportX),
                    static_cast<int32>(Draw.ViewportY),
                    static_cast<int32>(Draw.ViewportX + Draw.ViewportW),
                    static_cast<int32>(Draw.ViewportY + Draw.ViewportH)});
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
            Commands.ExecuteIndirect(*Draw.Batch->IndirectArguments, Draw.Batch->MeshletCount);
        }
        if (OpenTarget != nullptr)
        {
            Commands.EndRenderPass();
            Commands.UseAsShaderResource(*OpenTarget);
        }
        return true;
    }
}
