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
        constexpr uint32 PointShadowCapacity = 4;
        constexpr uint32 PointFaceCount = 6;
        constexpr uint32 AreaShadowCapacity = 1;
        constexpr uint32 AreaShadowBase = ClipLevelCount + PointShadowCapacity * PointFaceCount;
        constexpr uint32 ShadowLevelCount = AreaShadowBase + AreaShadowCapacity;
        const float AreaShadowTanHalf = std::tan(GLMAdapter::Radians(60.0F));
        constexpr uint32 PageTableWidth = PagesPerAxis;
        constexpr uint32 PageTableHeight = PagesPerAxis * ShadowLevelCount;
        constexpr float PointShadowNear = 0.05F;
        constexpr uint32 ShadowConstantSlots = 4096;
        constexpr uint32 ShadowConstantStride = 1024;
        constexpr float LevelWorldSize[ClipLevelCount] = {8.0F, 16.0F, 32.0F, 64.0F};

        struct GpuShadowConstants
        {
            float ViewProjection[16];
            float World[16];
        };

        struct ShadowDraw
        {
            const MeshBatch* Batch = nullptr;
            float ViewProjection[16] = {};
            float World[16] = {};
            float NormalMatrix[16] = {};
            RHITexture* Target = nullptr;
            RHIPipeline* Pipeline = nullptr;
            float ViewportX = 0.0F;
            float ViewportY = 0.0F;
            float ViewportW = 0.0F;
            float ViewportH = 0.0F;
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
            if (Batch.Surface == EMaterialSurface::Translucent || Batch.VertexBuffer == nullptr || Batch.Meshlets == nullptr)
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

        void AddClear(
            std::vector<ShadowDraw>& Draws,
            RHITexture& Target,
            RHIPipeline& Pipeline)
        {
            ShadowDraw Draw;
            Draw.Target = &Target;
            Draw.Pipeline = &Pipeline;
            Draw.bBegin = true;
            Draws.push_back(Draw);
        }

        const Vec3 FaceForward[PointFaceCount] = {
            {1.0F, 0.0F, 0.0F},
            {-1.0F, 0.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
            {0.0F, -1.0F, 0.0F},
            {0.0F, 0.0F, 1.0F},
            {0.0F, 0.0F, -1.0F},
        };
        const Vec3 FaceUp[PointFaceCount] = {
            {0.0F, 1.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
            {0.0F, 0.0F, -1.0F},
            {0.0F, 0.0F, 1.0F},
            {0.0F, 1.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
        };

        float AreaShadowFar(const AreaLightRenderProxy& Light)
        {
            const float HalfDiagonal = 0.5F * std::sqrt(Light.Width * Light.Width + Light.Height * Light.Height);
            return std::max(Light.Radius + HalfDiagonal, PointShadowNear + 0.1F);
        }

        bool CollectWorldCasters(
            std::vector<VirtualShadows::WorldCaster>& CastersOut,
            const std::vector<MeshBatch>& Batches,
            const std::vector<MeshRenderProxy>& Casters,
            const AssetManager& Assets)
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
                    if (Batch == nullptr || Batch->Surface == EMaterialSurface::Translucent || Batch->VertexBuffer == nullptr ||
                        Batch->Meshlets == nullptr)
                    {
                        continue;
                    }

                    VirtualShadows::WorldCaster Bound;
                    Bound.Batch = Batch;
                    std::memcpy(Bound.World, Caster.World, sizeof(Bound.World));
                    std::memcpy(Bound.NormalMatrix, Caster.NormalMatrix, sizeof(Bound.NormalMatrix));
                    Bound.bTwoSided = Batch->bTwoSided;
                    bool bAny = false;
                    for (uint32 Corner = 0; Corner < 8; ++Corner)
                    {
                        const Vec3 WorldPoint = TransformPoint(
                            Caster.World,
                            (Corner & 1U) != 0U ? Sections[Section].BoundsMax[0] : Sections[Section].BoundsMin[0],
                            (Corner & 2U) != 0U ? Sections[Section].BoundsMax[1] : Sections[Section].BoundsMin[1],
                            (Corner & 4U) != 0U ? Sections[Section].BoundsMax[2] : Sections[Section].BoundsMin[2]);
                        if (!bAny)
                        {
                            Bound.Min[0] = WorldPoint.x;
                            Bound.Min[1] = WorldPoint.y;
                            Bound.Min[2] = WorldPoint.z;
                            Bound.Max[0] = WorldPoint.x;
                            Bound.Max[1] = WorldPoint.y;
                            Bound.Max[2] = WorldPoint.z;
                            bAny = true;
                        }
                        else
                        {
                            Bound.Min[0] = std::min(Bound.Min[0], WorldPoint.x);
                            Bound.Min[1] = std::min(Bound.Min[1], WorldPoint.y);
                            Bound.Min[2] = std::min(Bound.Min[2], WorldPoint.z);
                            Bound.Max[0] = std::max(Bound.Max[0], WorldPoint.x);
                            Bound.Max[1] = std::max(Bound.Max[1], WorldPoint.y);
                            Bound.Max[2] = std::max(Bound.Max[2], WorldPoint.z);
                        }
                    }
                    if (bAny)
                    {
                        CastersOut.push_back(Bound);
                    }
                }
            }
            return true;
        }

        bool OverlapsLight(const Vec3& Light, float Radius, const float Min[3], const float Max[3])
        {
            const float X = std::clamp(Light.x, Min[0], Max[0]);
            const float Y = std::clamp(Light.y, Min[1], Max[1]);
            const float Z = std::clamp(Light.z, Min[2], Max[2]);
            const Vec3 Delta(Light.x - X, Light.y - Y, Light.z - Z);
            return GLMAdapter::Dot(Delta, Delta) <= Radius * Radius;
        }

        bool ContainsLight(const Vec3& Light, const float Min[3], const float Max[3])
        {
            return Light.x >= Min[0] && Light.x <= Max[0] && Light.y >= Min[1] && Light.y <= Max[1] && Light.z >= Min[2] &&
                Light.z <= Max[2];
        }

        bool FaceUvBounds(
            const Mat4& View,
            const float Min[3],
            const float Max[3],
            float FarPlane,
            float TanHalf,
            float& OutU0,
            float& OutV0,
            float& OutU1,
            float& OutV1)
        {
            Vec3 ViewMin(0.0F);
            Vec3 ViewMax(0.0F);
            bool bAny = false;
            for (uint32 Corner = 0; Corner < 8; ++Corner)
            {
                const Vec3 ViewPoint = Vec3(View * Vec4(
                    (Corner & 1U) != 0U ? Max[0] : Min[0],
                    (Corner & 2U) != 0U ? Max[1] : Min[1],
                    (Corner & 4U) != 0U ? Max[2] : Min[2],
                    1.0F));
                if (!bAny)
                {
                    ViewMin = ViewPoint;
                    ViewMax = ViewPoint;
                    bAny = true;
                }
                else
                {
                    ViewMin = GLMAdapter::Min(ViewMin, ViewPoint);
                    ViewMax = GLMAdapter::Max(ViewMax, ViewPoint);
                }
            }
            if (!bAny || ViewMin.z > -PointShadowNear || ViewMax.z < -FarPlane)
            {
                return false;
            }

            const float ZLo = std::max(ViewMin.z, -FarPlane);
            const float ZHi = std::min(ViewMax.z, -PointShadowNear);
            if (ZLo > ZHi)
            {
                return false;
            }

            bool bUv = false;
            for (uint32 XBit = 0; XBit < 2; ++XBit)
            {
                for (uint32 YBit = 0; YBit < 2; ++YBit)
                {
                    for (uint32 ZBit = 0; ZBit < 2; ++ZBit)
                    {
                        const float X = XBit == 0U ? ViewMin.x : ViewMax.x;
                        const float Y = YBit == 0U ? ViewMin.y : ViewMax.y;
                        const float Z = ZBit == 0U ? ZLo : ZHi;
                        const float NdcX = (X / -Z) / TanHalf;
                        const float NdcY = (Y / -Z) / TanHalf;
                        const float U = NdcX * 0.5F + 0.5F;
                        const float V = -NdcY * 0.5F + 0.5F;
                        if (!bUv)
                        {
                            OutU0 = U;
                            OutV0 = V;
                            OutU1 = U;
                            OutV1 = V;
                            bUv = true;
                        }
                        else
                        {
                            OutU0 = std::min(OutU0, U);
                            OutV0 = std::min(OutV0, V);
                            OutU1 = std::max(OutU1, U);
                            OutV1 = std::max(OutV1, V);
                        }
                    }
                }
            }
            if (!bUv || OutU1 < 0.0F || OutV1 < 0.0F || OutU0 > 1.0F || OutV0 > 1.0F)
            {
                return false;
            }
            OutU0 = std::clamp(OutU0, 0.0F, 1.0F);
            OutV0 = std::clamp(OutV0, 0.0F, 1.0F);
            OutU1 = std::clamp(OutU1, 0.0F, 1.0F);
            OutV1 = std::clamp(OutV1, 0.0F, 1.0F);
            return OutU0 < OutU1 && OutV0 < OutV1;
        }

        Mat4 PageProjection(const Mat4& FaceClip, uint32 PageX, uint32 PageY)
        {
            const float U0 = static_cast<float>(PageX) / static_cast<float>(PagesPerAxis);
            const float U1 = static_cast<float>(PageX + 1) / static_cast<float>(PagesPerAxis);
            const float V0 = static_cast<float>(PageY) / static_cast<float>(PagesPerAxis);
            const float V1 = static_cast<float>(PageY + 1) / static_cast<float>(PagesPerAxis);
            const float X0 = U0 * 2.0F - 1.0F;
            const float X1 = U1 * 2.0F - 1.0F;
            const float Y0 = 1.0F - V1 * 2.0F;
            const float Y1 = 1.0F - V0 * 2.0F;
            const float ScaleX = 2.0F / (X1 - X0);
            const float ScaleY = 2.0F / (Y1 - Y0);
            const float OffsetX = -(X0 + X1) / (X1 - X0);
            const float OffsetY = -(Y0 + Y1) / (Y1 - Y0);
            const float Scale[16] = {
                ScaleX, 0.0F, 0.0F, 0.0F,
                0.0F, ScaleY, 0.0F, 0.0F,
                0.0F, 0.0F, 1.0F, 0.0F,
                OffsetX, OffsetY, 0.0F, 1.0F,
            };
            return GLMAdapter::MakeMat4(Scale) * FaceClip;
        }
    }

    VirtualShadows::VirtualShadows() = default;
    VirtualShadows::~VirtualShadows() = default;

    std::unique_ptr<RHIPipeline> VirtualShadows::CreateDepthPipeline(
        RHIDevice& Device,
        ShaderManager& Shaders,
        const char* ShaderPath,
        const char* MeshShaderPath,
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
        RHIShader* MeshShader = Shaders.LoadStandalone(Device, MeshShaderPath, "MeshMain", ERHIShaderStage::Mesh, ErrorMessage);
        if (MeshShader == nullptr)
        {
            return nullptr;
        }

        RHIGraphicsPipelineDesc PipelineDesc;
        PipelineDesc.MeshShader = MeshShader;
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
        static_assert(sizeof(GpuShadowConstants) == 128);
        static_assert(sizeof(GpuShadowConstants) <= ShadowConstantStride);
        if (bReady)
        {
            return true;
        }

        DirectionalPipeline = CreateDepthPipeline(
            Device,
            Shaders,
            "Depth/DirectionalShadow.slang",
            "Mesh/PositionMesh.slang",
            ERHICullMode::Back,
            ErrorMessage);
        DirectionalTwoSidedPipeline = CreateDepthPipeline(
            Device,
            Shaders,
            "Depth/DirectionalShadow.slang",
            "Mesh/PositionMesh.slang",
            ERHICullMode::None,
            ErrorMessage);
        if (DirectionalPipeline == nullptr || DirectionalTwoSidedPipeline == nullptr)
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

        PageSampler = Device.CreatePointClampSampler(ErrorMessage);
        ConstantBuffer = Device.CreateConstantBuffer(ShadowConstantSlots * ShadowConstantStride, ErrorMessage);
        if (PageSampler == nullptr || ConstantBuffer == nullptr)
        {
            return false;
        }

        PageSamplerDescriptor = PageSampler->GetDescriptorIndex();
        bReady = true;
        Log::Info(
            "Virtual shadows ready: {} clip levels, {} point faces, {} area face, page {}, atlas {}x{}",
            ClipLevelCount,
            PointFaceCount,
            AreaShadowCapacity,
            PageSize,
            AtlasWidth,
            AtlasHeight);
        return true;
    }

    bool VirtualShadows::Prepare(
        const std::vector<MeshBatch>& Batches,
        const std::vector<MeshRenderProxy>& Casters,
        const AssetManager& Assets,
        const Camera& View,
        const DirectLightRenderProxy* DirectLight,
        const std::vector<PointLightRenderProxy>& PointLights,
        const std::vector<AreaLightRenderProxy>& AreaLights,
        std::string* ErrorMessage)
    {
        if (!bReady)
        {
            SetErrorMessage(ErrorMessage, "Virtual shadow maps are not ready");
            return false;
        }

        bDirectActive = false;
        Pages.clear();
        CasterBounds.clear();
        WorldCasters.clear();
        ActivePoints[0] = {};
        ActivePoints[1] = {};
        ActivePoints[2] = {};
        ActivePoints[3] = {};
        ActiveAreas[0] = {};
        std::fill(PagePixels.begin(), PagePixels.end(), static_cast<uint8>(0));
        for (ClipLevel& Level : Levels)
        {
            Level = {};
        }

        const UIManager::LightState LightSettings = UIManager::Get().GetState().Light;
        Binding = {};
        Binding.Settings[0] = static_cast<float>(ShadowLevelCount);
        Binding.Settings[1] = LightSettings.ShadowNormalBias;
        Binding.VirtualInfo[0] = static_cast<float>(PagesPerAxis);
        Binding.VirtualInfo[1] = static_cast<float>(PageSize);
        Binding.VirtualInfo[2] = static_cast<float>(PhysicalPagesX);
        Binding.VirtualInfo[3] = static_cast<float>(PhysicalPagesY);

        const Vec3 RawDirection = DirectLight == nullptr
            ? Vec3(0.0F)
            : Vec3(DirectLight->Direction[0], DirectLight->Direction[1], DirectLight->Direction[2]);
        const bool bMarkDirectional = LightSettings.bDirectionalShadow && DirectLight != nullptr && DirectLight->Intensity > 0.0F &&
            GLMAdapter::Length(RawDirection) >= 0.0001F;
        uint32 Physical = 0;
        if (bMarkDirectional)
        {

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
        }

        if (!CollectWorldCasters(WorldCasters, Batches, Casters, Assets))
        {
            return false;
        }
        const auto MarkPerspectiveFace = [&](const Mat4& ViewMatrix, const Mat4& FaceClip, const Vec3& Eye, float FarPlane, float TanHalf, uint32 LevelIndex)
        {
            bool Marked[PagesPerAxis][PagesPerAxis] = {};
            for (const WorldCaster& Caster : WorldCasters)
            {
                if (!OverlapsLight(Eye, FarPlane, Caster.Min, Caster.Max))
                {
                    continue;
                }

                float U0 = 0.0F;
                float V0 = 0.0F;
                float U1 = 1.0F;
                float V1 = 1.0F;
                if (ContainsLight(Eye, Caster.Min, Caster.Max))
                {
                    U0 = 0.0F;
                    V0 = 0.0F;
                    U1 = 1.0F;
                    V1 = 1.0F;
                }
                else if (!FaceUvBounds(ViewMatrix, Caster.Min, Caster.Max, FarPlane, TanHalf, U0, V0, U1, V1))
                {
                    continue;
                }

                const int32 PageMinX = std::clamp(static_cast<int32>(std::floor(U0 * static_cast<float>(PagesPerAxis))), 0, static_cast<int32>(PagesPerAxis - 1));
                const int32 PageMinY = std::clamp(static_cast<int32>(std::floor(V0 * static_cast<float>(PagesPerAxis))), 0, static_cast<int32>(PagesPerAxis - 1));
                const int32 PageMaxX = std::clamp(static_cast<int32>(std::floor(std::min(U1, 0.9999F) * static_cast<float>(PagesPerAxis))), 0, static_cast<int32>(PagesPerAxis - 1));
                const int32 PageMaxY = std::clamp(static_cast<int32>(std::floor(std::min(V1, 0.9999F) * static_cast<float>(PagesPerAxis))), 0, static_cast<int32>(PagesPerAxis - 1));
                for (int32 PageY = PageMinY; PageY <= PageMaxY; ++PageY)
                {
                    for (int32 PageX = PageMinX; PageX <= PageMaxX; ++PageX)
                    {
                        Marked[PageX][PageY] = true;
                    }
                }
            }

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
                    Page.MinX = static_cast<float>(PageX) / static_cast<float>(PagesPerAxis);
                    Page.MinY = static_cast<float>(PageY) / static_cast<float>(PagesPerAxis);
                    Page.MaxX = static_cast<float>(PageX + 1) / static_cast<float>(PagesPerAxis);
                    Page.MaxY = static_cast<float>(PageY + 1) / static_cast<float>(PagesPerAxis);
                    const Mat4 DrawClip = PageProjection(FaceClip, PageX, PageY);
                    std::memcpy(Page.DrawClip, GLMAdapter::ValuePtr(DrawClip), sizeof(Page.DrawClip));
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
        };

        const size_t PointCount = std::min(PointLights.size(), static_cast<size_t>(PointShadowCapacity));
        for (size_t LightIndex = 0; LightIndex < PointCount; ++LightIndex)
        {
            const PointLightRenderProxy& Point = PointLights[LightIndex];
            if (Point.Intensity <= 0.0F || Point.Radius <= PointShadowNear)
            {
                continue;
            }

            ActivePoints[LightIndex] = Point;
            const Vec3 Eye(Point.Position[0], Point.Position[1], Point.Position[2]);
            const float FarPlane = std::max(Point.Radius, PointShadowNear + 0.1F);
            const Mat4 Projection = GLMAdapter::PerspectiveRH_ZO(GLMAdapter::Radians(90.0F), 1.0F, PointShadowNear, FarPlane);
            for (uint32 Face = 0; Face < PointFaceCount; ++Face)
            {
                const Mat4 ViewMatrix = GLMAdapter::LookAtRH(Eye, Eye + FaceForward[Face], FaceUp[Face]);
                const uint32 LevelIndex = ClipLevelCount + static_cast<uint32>(LightIndex) * PointFaceCount + Face;
                MarkPerspectiveFace(ViewMatrix, Projection * ViewMatrix, Eye, FarPlane, 1.0F, LevelIndex);
            }
        }

        const size_t AreaCount = std::min(AreaLights.size(), static_cast<size_t>(AreaShadowCapacity));
        for (size_t LightIndex = 0; LightIndex < AreaCount; ++LightIndex)
        {
            const AreaLightRenderProxy& Area = AreaLights[LightIndex];
            if (Area.Intensity <= 0.0F || Area.Radius <= PointShadowNear)
            {
                continue;
            }

            ActiveAreas[LightIndex] = Area;
            const Vec3 Eye(Area.Position[0], Area.Position[1], Area.Position[2]);
            const Vec3 Forward(Area.Direction[0], Area.Direction[1], Area.Direction[2]);
            const Vec3 Up(Area.Tangent[0], Area.Tangent[1], Area.Tangent[2]);
            const float FarPlane = AreaShadowFar(Area);
            const Mat4 Projection = GLMAdapter::PerspectiveRH_ZO(GLMAdapter::Radians(120.0F), 1.0F, PointShadowNear, FarPlane);
            const Mat4 ViewMatrix = GLMAdapter::LookAtRH(Eye, Eye + Forward, Up);
            MarkPerspectiveFace(
                ViewMatrix,
                Projection * ViewMatrix,
                Eye,
                FarPlane,
                AreaShadowTanHalf,
                AreaShadowBase + static_cast<uint32>(LightIndex));
        }

        if (!Pages.empty())
        {
            const float WorldTexel = LevelWorldSize[0] / static_cast<float>(PagesPerAxis * PageSize);
            Binding.Settings[1] = std::max(WorldTexel * 3.0F, LightSettings.ShadowNormalBias);
            Binding.DirectMap = AtlasDescriptor;
            Binding.PageTable = PageDescriptor;
            Binding.PageSampler = PageSamplerDescriptor;
            bDirectActive = true;
        }
        return true;
    }

    bool VirtualShadows::Record(
        RHICommandList& Commands,
        const std::vector<MeshBatch>&,
        const std::vector<MeshRenderProxy>&)
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
            AddClear(Draws, *Atlas, *DirectionalPipeline);
            const Mat4 ViewMatrix = GLMAdapter::MakeMat4(LightView);
            for (const VirtualPage& Page : Pages)
            {
                float ViewProjection[16];
                const float OriginX = static_cast<float>((Page.Physical % PhysicalPagesX) * PageSize);
                const float OriginY = static_cast<float>((Page.Physical / PhysicalPagesX) * PageSize);
                bool bFirst = true;
                if (Page.Level < ClipLevelCount)
                {
                    const Mat4 Clip = GLMAdapter::OrthoRH_ZO(
                        Page.MinX,
                        Page.MaxX,
                        Page.MinY,
                        Page.MaxY,
                        Levels[Page.Level].Near,
                        Levels[Page.Level].Far) * ViewMatrix;
                    std::memcpy(ViewProjection, GLMAdapter::ValuePtr(Clip), sizeof(ViewProjection));
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
                    continue;
                }

                std::memcpy(ViewProjection, Page.DrawClip, sizeof(ViewProjection));
                Vec3 Eye(0.0F);
                float FarPlane = PointShadowNear + 0.1F;
                float TanHalf = 1.0F;
                Mat4 FaceView(1.0F);
                if (Page.Level < AreaShadowBase)
                {
                    const uint32 PointSlot = (Page.Level - ClipLevelCount) / PointFaceCount;
                    const uint32 Face = (Page.Level - ClipLevelCount) % PointFaceCount;
                    const PointLightRenderProxy& Point = ActivePoints[PointSlot];
                    Eye = Vec3(Point.Position[0], Point.Position[1], Point.Position[2]);
                    FarPlane = std::max(Point.Radius, PointShadowNear + 0.1F);
                    FaceView = GLMAdapter::LookAtRH(Eye, Eye + FaceForward[Face], FaceUp[Face]);
                }
                else
                {
                    const AreaLightRenderProxy& Area = ActiveAreas[Page.Level - AreaShadowBase];
                    Eye = Vec3(Area.Position[0], Area.Position[1], Area.Position[2]);
                    FarPlane = AreaShadowFar(Area);
                    TanHalf = AreaShadowTanHalf;
                    const Vec3 Forward(Area.Direction[0], Area.Direction[1], Area.Direction[2]);
                    const Vec3 Up(Area.Tangent[0], Area.Tangent[1], Area.Tangent[2]);
                    FaceView = GLMAdapter::LookAtRH(Eye, Eye + Forward, Up);
                }
                for (const WorldCaster& Bound : WorldCasters)
                {
                    float U0 = 0.0F;
                    float V0 = 0.0F;
                    float U1 = 0.0F;
                    float V1 = 0.0F;
                    const bool bInside = ContainsLight(Eye, Bound.Min, Bound.Max);
                    if (!bInside && !FaceUvBounds(FaceView, Bound.Min, Bound.Max, FarPlane, TanHalf, U0, V0, U1, V1))
                    {
                        continue;
                    }
                    if (!bInside && (U1 < Page.MinX || U0 > Page.MaxX || V1 < Page.MinY || V0 > Page.MaxY))
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
            }
            else
            {
                std::memcpy(Destination, Draw.ViewProjection, sizeof(Draw.ViewProjection));
                std::memcpy(Destination + 64, Draw.World, sizeof(Draw.World));
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
                Commands.BeginDepthPass(*Draw.Target, true, 0);
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

            DrawMeshBatch(
                Commands,
                *Draw.Batch,
                *Draw.Pipeline,
                *ConstantBuffer,
                static_cast<uint32>(Index) * ShadowConstantStride);
        }
        if (OpenTarget != nullptr)
        {
            Commands.EndRenderPass();
            Commands.UseAsShaderResource(*OpenTarget);
        }
        return true;
    }
}
