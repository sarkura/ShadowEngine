#include "Framework/Render/Public/Renderer.h"

#include "Framework/Asset/Public/AssetManager.h"
#include "Framework/Asset/Public/MeshAsset.h"
#include "Framework/Asset/Public/TextureAsset.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/Material/Public/Material.h"
#include "Framework/Material/Public/MaterialInstance.h"
#include "Framework/Material/Public/MaterialParameter.h"
#include "Framework/Render/Public/RenderQueue.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/RHI/Public/RHIPipelineLayout.h"
#include "Framework/RHI/Public/RHISwapChain.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace ShadowEngine
{
    namespace
    {
        std::span<const uint8> AsBytes(const void* Data, size_t Size)
        {
            return {static_cast<const uint8*>(Data), Size};
        }

        void BuildFrustumPlanes(const glm::mat4& ViewProjection, float Planes[6][4])
        {
            const glm::vec4 Row0(ViewProjection[0][0], ViewProjection[1][0], ViewProjection[2][0], ViewProjection[3][0]);
            const glm::vec4 Row1(ViewProjection[0][1], ViewProjection[1][1], ViewProjection[2][1], ViewProjection[3][1]);
            const glm::vec4 Row2(ViewProjection[0][2], ViewProjection[1][2], ViewProjection[2][2], ViewProjection[3][2]);
            const glm::vec4 Row3(ViewProjection[0][3], ViewProjection[1][3], ViewProjection[2][3], ViewProjection[3][3]);
            const glm::vec4 Raw[6] = {
                Row3 + Row0,
                Row3 - Row0,
                Row3 + Row1,
                Row3 - Row1,
                Row2,
                Row3 - Row2,
            };
            for (uint32 Index = 0; Index < 6; ++Index)
            {
                const float Length = glm::length(glm::vec3(Raw[Index]));
                const glm::vec4 Plane = Length > 0.0001F ? Raw[Index] / Length : Raw[Index];
                Planes[Index][0] = Plane.x;
                Planes[Index][1] = Plane.y;
                Planes[Index][2] = Plane.z;
                Planes[Index][3] = Plane.w;
            }
        }

        bool SphereIntersectsFrustum(const float Planes[6][4], const glm::vec3& Center, float Radius)
        {
            for (uint32 Index = 0; Index < 6; ++Index)
            {
                const glm::vec3 Normal(Planes[Index][0], Planes[Index][1], Planes[Index][2]);
                if (glm::dot(Normal, Center) + Planes[Index][3] < -Radius)
                {
                    return false;
                }
            }

            return true;
        }

        void WorldBounds(const float World[16], const float LocalMin[3], const float LocalMax[3], glm::vec3& OutMin, glm::vec3& OutMax)
        {
            const glm::mat4 Matrix = glm::make_mat4(World);
            const glm::vec3 Corner(LocalMin[0], LocalMin[1], LocalMin[2]);
            const glm::vec3 Opposite(LocalMax[0], LocalMax[1], LocalMax[2]);
            OutMin = glm::vec3(std::numeric_limits<float>::max());
            OutMax = glm::vec3(std::numeric_limits<float>::lowest());
            for (int X = 0; X < 2; ++X)
            {
                for (int Y = 0; Y < 2; ++Y)
                {
                    for (int Z = 0; Z < 2; ++Z)
                    {
                        const glm::vec3 Local(X == 0 ? Corner.x : Opposite.x, Y == 0 ? Corner.y : Opposite.y, Z == 0 ? Corner.z : Opposite.z);
                        const glm::vec3 Point = glm::vec3(Matrix * glm::vec4(Local, 1.0F));
                        OutMin = glm::min(OutMin, Point);
                        OutMax = glm::max(OutMax, Point);
                    }
                }
            }
        }

        bool AabbIntersectsFrustum(const float Planes[6][4], const glm::vec3& Min, const glm::vec3& Max)
        {
            const glm::vec3 Center = (Min + Max) * 0.5F;
            const glm::vec3 Extent = (Max - Min) * 0.5F;
            for (uint32 Index = 0; Index < 6; ++Index)
            {
                const glm::vec3 Normal(Planes[Index][0], Planes[Index][1], Planes[Index][2]);
                const glm::vec3 Positive(
                    Normal.x >= 0.0F ? Extent.x : -Extent.x,
                    Normal.y >= 0.0F ? Extent.y : -Extent.y,
                    Normal.z >= 0.0F ? Extent.z : -Extent.z);
                if (glm::dot(Normal, Center + Positive) + Planes[Index][3] < 0.0F)
                {
                    return false;
                }
            }

            return true;
        }
    }

    bool Renderer::CollectVisibleItems(const RenderItemFilter& Filter, RenderQueue& Queue)
    {
        FrameFrustum = {};
        if (Assets == nullptr || SwapChain == nullptr)
        {
            return false;
        }

        if (!FillRenderQueue(SceneProxy, Filter, *Assets, GetName(), MeshBatches, Queue))
        {
            return false;
        }

        const uint32 Width = SwapChain->GetWidth();
        const uint32 Height = SwapChain->GetHeight();
        if (Width == 0 || Height == 0)
        {
            return true;
        }

        const float Aspect = static_cast<float>(Width) / static_cast<float>(Height);
        const glm::mat4 Projection = glm::perspectiveRH_ZO(glm::radians(45.0F), Aspect, 0.1F, 100.0F);
        BuildFrustumPlanes(Projection * ViewCamera.ViewMatrix(), FrameFrustum.Planes);
        FrameFrustum.bValid = true;

        std::vector<RenderItem> Visible;
        Visible.reserve(Queue.GetItems().size());
        for (const RenderItem& Item : Queue.GetItems())
        {
            const MeshAsset* Mesh = Assets->ResolveMesh(Item.Mesh);
            if (Mesh == nullptr || Item.Section >= Mesh->GetSections().size())
            {
                Visible.push_back(Item);
                continue;
            }

            const MeshSection& Section = Mesh->GetSections()[Item.Section];
            glm::vec3 Min;
            glm::vec3 Max;
            WorldBounds(Item.World, Section.BoundsMin, Section.BoundsMax, Min, Max);
            if (AabbIntersectsFrustum(FrameFrustum.Planes, Min, Max))
            {
                Visible.push_back(Item);
            }
        }
        Queue.Assign(std::move(Visible));
        return true;
    }

    void Renderer::SelectVisiblePointLights(uint32 Limit, std::vector<PointLightRenderProxy>& Out) const
    {
        Out.clear();
        if (!FrameFrustum.bValid)
        {
            return;
        }

        for (const PointLightRenderProxy& Light : SceneProxy.PointLights)
        {
            const glm::vec3 Center(Light.Position[0], Light.Position[1], Light.Position[2]);
            if (SphereIntersectsFrustum(FrameFrustum.Planes, Center, Light.Radius))
            {
                Out.push_back(Light);
            }
        }

        if (Out.size() <= Limit)
        {
            return;
        }

        static size_t LoggedCount = 0;
        if (LoggedCount != Out.size())
        {
            Log::Warning("View contains {} point lights and keeps the first {}", Out.size(), Limit);
            LoggedCount = Out.size();
        }
        Out.resize(Limit);
    }

    Renderer::~Renderer()
    {
        Finalize();
    }

    ERHIFormat Renderer::GetSceneColorFormat() const
    {
        return SwapChain != nullptr ? SwapChain->GetFormat() : ERHIFormat::R8G8B8A8_UNorm;
    }

    bool Renderer::Initialize(
        RHIDevice& InDevice,
        RHISwapChain& InSwapChain,
        ShaderManager& Shaders,
        AssetManager& InAssets,
        const Scene& InScene,
        const RHIColor& InClearColor,
        std::string* ErrorMessage)
    {
        Device = &InDevice;
        SwapChain = &InSwapChain;
        Assets = &InAssets;
        ShaderLibrary = &Shaders;
        ClearColor = InClearColor;
        Sync(InScene);

        std::vector<MeshHandle> UniqueMeshes;
        for (const MeshRenderProxy& Proxy : SceneProxy.Meshes)
        {
            if (!Proxy.Mesh.IsValid())
            {
                continue;
            }

            bool bUploaded = false;
            for (const MeshHandle& Existing : UniqueMeshes)
            {
                if (Existing.Index == Proxy.Mesh.Index)
                {
                    bUploaded = true;
                    break;
                }
            }
            if (!bUploaded)
            {
                UniqueMeshes.push_back(Proxy.Mesh);
            }
        }

        if (UniqueMeshes.empty())
        {
            SetErrorMessage(ErrorMessage, "Scene has no mesh asset");
            return false;
        }

        for (const MeshHandle& Mesh : UniqueMeshes)
        {
            if (!UploadMesh(Shaders, Mesh, ErrorMessage))
            {
                return false;
            }
        }

        uint32 DrawCount = 0;
        for (const MeshRenderProxy& Proxy : SceneProxy.Meshes)
        {
            const MeshAsset* Mesh = Assets->ResolveMesh(Proxy.Mesh);
            if (Mesh != nullptr)
            {
                DrawCount += static_cast<uint32>(Mesh->GetSections().size());
            }
        }
        ConstantCapacity = DrawCount == 0 ? 1 : DrawCount;
        ConstantBuffer = InDevice.CreateConstantBuffer(ConstantCapacity * ConstantAlignment, ErrorMessage);
        if (ConstantBuffer == nullptr)
        {
            return false;
        }

        if (!Resize(InSwapChain.GetWidth(), InSwapChain.GetHeight(), ErrorMessage))
        {
            return false;
        }

        if (DrawsSkyBackground() && !UploadEnvironment(Shaders, ErrorMessage))
        {
            return false;
        }

        CommandList = InDevice.CreateCommandList(ErrorMessage);
        return CommandList != nullptr;
    }

    bool Renderer::Resize(uint32 Width, uint32 Height, std::string* ErrorMessage)
    {
        if (Device == nullptr)
        {
            SetErrorMessage(ErrorMessage, "Renderer is not initialized");
            return false;
        }

        auto NewDepth = Device->CreateDepthTexture(Width, Height, ErrorMessage);
        if (NewDepth == nullptr)
        {
            return false;
        }

        DepthBuffer = std::move(NewDepth);
        return true;
    }

    bool Renderer::UploadMesh(ShaderManager& Shaders, MeshHandle Handle, std::string* ErrorMessage)
    {
        const MeshAsset* Found = Assets->ResolveMesh(Handle);
        if (Found == nullptr)
        {
            SetErrorMessage(ErrorMessage, "Mesh handle is invalid");
            return false;
        }

        const MeshAsset& Mesh = *Found;
        if (Mesh.GetSections().empty())
        {
            SetErrorMessage(ErrorMessage, "Mesh has no sections: " + Mesh.GetPath().generic_string());
            return false;
        }

        const std::vector<MeshSection>& Sections = Mesh.GetSections();
        for (uint32 SectionIndex = 0; SectionIndex < static_cast<uint32>(Sections.size()); ++SectionIndex)
        {
            const MeshSection& Section = Sections[SectionIndex];
            const MaterialInstance* Instance = Assets->ResolveMaterialInstance(Section.Material);
            const Material* Material = Instance == nullptr ? nullptr : Assets->ResolveMaterial(Instance->GetBaseMaterial());
            if (Material == nullptr)
            {
                SetErrorMessage(ErrorMessage, "Mesh section has no material: " + Mesh.GetPath().generic_string());
                return false;
            }

            const ShaderHandle* Shader = Material->FindShader(GetName());
            if (Shader == nullptr)
            {
                continue;
            }

            if (!Shaders.LoadShaders(*Device, *Shader, ErrorMessage))
            {
                return false;
            }

            const std::vector<MeshVertex>& Vertices = Section.Vertices;
            const std::vector<uint32>& Indices = Section.Indices;
            if (Vertices.empty() || Indices.empty())
            {
                SetErrorMessage(ErrorMessage, "Mesh section has no triangles: " + Mesh.GetPath().generic_string());
                return false;
            }

            MeshBatch Batch;
            Batch.Mesh = Handle;
            Batch.Section = SectionIndex;
            Batch.VertexShader = Shaders.GetVertexShader(*Shader);
            Batch.PixelShader = Shaders.GetPixelShader(*Shader);
            if (Batch.VertexShader == nullptr || Batch.PixelShader == nullptr)
            {
                SetErrorMessage(ErrorMessage, "Material shader is not loaded: " + Mesh.GetPath().generic_string());
                return false;
            }

            RHIBufferDesc VertexDesc;
            VertexDesc.Stride = sizeof(MeshVertex);
            VertexDesc.Data = AsBytes(Vertices.data(), Vertices.size() * sizeof(MeshVertex));
            Batch.VertexBuffer = Device->CreateVertexBuffer(VertexDesc, ErrorMessage);
            if (Batch.VertexBuffer == nullptr)
            {
                return false;
            }

            const uint32 MaxIndex = *std::max_element(Indices.begin(), Indices.end());
            const bool bUint16 = MaxIndex <= 0xFFFF;
            std::vector<uint16> Indices16;
            RHIBufferDesc IndexDesc;
            if (bUint16)
            {
                Indices16.reserve(Indices.size());
                for (const uint32 Index : Indices)
                {
                    Indices16.push_back(static_cast<uint16>(Index));
                }
                IndexDesc.Stride = sizeof(uint16);
                IndexDesc.Data = AsBytes(Indices16.data(), Indices16.size() * sizeof(uint16));
                Batch.IndexBuffer = Device->CreateIndexBuffer(IndexDesc, ERHIIndexFormat::Uint16, ErrorMessage);
            }
            else
            {
                IndexDesc.Stride = sizeof(uint32);
                IndexDesc.Data = AsBytes(Indices.data(), Indices.size() * sizeof(uint32));
                Batch.IndexBuffer = Device->CreateIndexBuffer(IndexDesc, ERHIIndexFormat::Uint32, ErrorMessage);
            }
            if (Batch.IndexBuffer == nullptr)
            {
                return false;
            }

            Batch.IndexCount = static_cast<uint32>(Indices.size());

            constexpr RHIInputElement InputLayout[] = {
                {"POSITION", 0, 0, ERHIVertexFormat::Float32x3},
                {"NORMAL", 0, 12, ERHIVertexFormat::Float32x3},
                {"COLOR", 0, 24, ERHIVertexFormat::Float32x3},
                {"TEXCOORD", 0, 36, ERHIVertexFormat::Float32x2},
            };

            const ShaderReflection* VertexReflection = Shaders.GetReflection(*Shader, ERHIShaderStage::Vertex);
            const ShaderReflection* PixelReflection = Shaders.GetReflection(*Shader, ERHIShaderStage::Pixel);
            if (VertexReflection == nullptr || PixelReflection == nullptr)
            {
                SetErrorMessage(ErrorMessage, "Shader reflection is missing: " + Mesh.GetPath().generic_string());
                return false;
            }

            Batch.Layout = RHIPipelineLayout::FromReflection(*VertexReflection, *PixelReflection);

            RHIGraphicsPipelineDesc PipelineDesc;
            PipelineDesc.VertexShader = Batch.VertexShader;
            PipelineDesc.PixelShader = Batch.PixelShader;
            PipelineDesc.Layout = &Batch.Layout;
            PipelineDesc.RenderTargetFormat = GetSceneColorFormat();
            PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
            PipelineDesc.InputLayout = InputLayout;
            PipelineDesc.bEnableDepth = true;
            if (UsesDepthPrepass())
            {
                PipelineDesc.bWriteDepth = false;
                PipelineDesc.bDepthEqual = true;
                PipelineDesc.DepthFormat = ERHIFormat::D32_Float;
            }
            Batch.Pipeline = Device->CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
            if (Batch.Pipeline == nullptr)
            {
                return false;
            }

            const MaterialParameterBlock* Parameter = Instance->FindParameters(GetName());
            const MaterialParameter::TextureBinding Textures = Parameter == nullptr
                ? MaterialParameter::TextureBinding{}
                : MaterialParameter::GetTextureBinding(*Parameter);
            if (!Textures.IsValid())
            {
                SetErrorMessage(ErrorMessage, "Material sampler does not match its textures: " + Mesh.GetPath().generic_string());
                return false;
            }

            for (const auto& [Name, Handle] : Textures.Textures)
            {
                (void)Name;
                if (Handle.IsValid() && UploadTexture(Handle, ErrorMessage) == nullptr)
                {
                    return false;
                }
            }
            if (Textures.Sampler.IsValid() && UploadSampler(Textures.Sampler, ErrorMessage) == nullptr)
            {
                return false;
            }

            MeshBatches.push_back(std::move(Batch));
        }

        return true;
    }

    RHITexture* Renderer::UploadTexture(TextureAssetHandle Handle, std::string* ErrorMessage)
    {
        const TextureAsset* Asset = Assets->ResolveTexture(Handle);
        if (Asset == nullptr)
        {
            SetErrorMessage(ErrorMessage, "Texture handle is invalid");
            return nullptr;
        }

        const auto Found = GpuTextures.find(Handle.Index);
        if (Found != GpuTextures.end())
        {
            return Found->second.get();
        }

        std::vector<RHITextureMipDesc> Mips;
        Mips.reserve(Asset->GetMips().size());
        for (const TextureAsset::Mip& Mip : Asset->GetMips())
        {
            RHITextureMipDesc Desc;
            Desc.Width = Mip.Width;
            Desc.Height = Mip.Height;
            Desc.Pixels = Mip.Pixels.data();
            Desc.Size = static_cast<uint32>(Mip.Pixels.size());
            Mips.push_back(Desc);
        }

        RHITextureDesc TextureDesc;
        if (Asset->GetFormat() == ETextureFormat::RGBA32_Float)
        {
            TextureDesc.Format = ERHIFormat::R32G32B32A32_Float;
        }
        else if (Asset->GetEncoding() == ETextureEncoding::sRGB)
        {
            TextureDesc.Format = ERHIFormat::R8G8B8A8_UNorm_sRGB;
        }
        else
        {
            TextureDesc.Format = ERHIFormat::R8G8B8A8_UNorm;
        }
        TextureDesc.Mips = Mips;
        std::unique_ptr<RHITexture> Texture = Device->CreateTexture(TextureDesc, ErrorMessage);
        if (Texture == nullptr)
        {
            return nullptr;
        }

        RHITexture* Loaded = Texture.get();
        uint32 Created = 0;
        if (!Device->CreateShaderResourceView(*Loaded, Created, ErrorMessage))
        {
            return nullptr;
        }
        TextureDescriptors.emplace(Handle.Index, Created);
        GpuTextures.emplace(Handle.Index, std::move(Texture));
        return Loaded;
    }

    void Renderer::ReleaseUploadedTexture(TextureAssetHandle Handle)
    {
        GpuTextures.erase(Handle.Index);
        TextureDescriptors.erase(Handle.Index);
    }

    RHISampler* Renderer::UploadSampler(SamplerHandle Handle, std::string* ErrorMessage)
    {
        if (!Handle.IsValid())
        {
            SetErrorMessage(ErrorMessage, "Sampler handle is invalid");
            return nullptr;
        }

        const auto Found = GpuSamplers.find(Handle.Index);
        if (Found != GpuSamplers.end())
        {
            return Found->second.get();
        }

        std::unique_ptr<RHISampler> Sampler = Device->CreateSampler(ErrorMessage);
        if (Sampler == nullptr)
        {
            return nullptr;
        }

        RHISampler* Loaded = Sampler.get();
        GpuSamplers.emplace(Handle.Index, std::move(Sampler));
        return Loaded;
    }

    bool Renderer::UploadEnvironment(ShaderManager& Shaders, std::string* ErrorMessage)
    {
        const TextureAssetHandle Environment = Assets->GetEnvironmentMap();
        RHITexture* Texture = UploadTexture(Environment, ErrorMessage);
        if (Texture == nullptr)
        {
            return false;
        }

        RHISampler* Sampler = nullptr;
        const auto ExistingSampler = GpuSamplers.find(0);
        if (ExistingSampler != GpuSamplers.end())
        {
            Sampler = ExistingSampler->second.get();
        }
        else
        {
            std::unique_ptr<RHISampler> Created = Device->CreateSampler(ErrorMessage);
            if (Created == nullptr)
            {
                return false;
            }

            Sampler = Created.get();
            GpuSamplers.emplace(0, std::move(Created));
        }

        const auto Descriptor = TextureDescriptors.find(Environment.Index);
        if (Descriptor == TextureDescriptors.end())
        {
            SetErrorMessage(ErrorMessage, "Environment map descriptor is missing");
            return false;
        }

        EnvironmentDescriptor = Descriptor->second;
        EnvironmentSampler = Sampler->GetDescriptorIndex();

        const ShaderHandle SkyShader = Shaders.Register(
            "Sky/SkyVertex.slang",
            "Sky/SkyFragment.slang",
            {},
            {});
        if (!SkyShader.IsValid() || !Shaders.LoadShaders(*Device, SkyShader, ErrorMessage))
        {
            return false;
        }

        const ShaderReflection* SkyVertex = Shaders.GetReflection(SkyShader, ERHIShaderStage::Vertex);
        const ShaderReflection* SkyPixel = Shaders.GetReflection(SkyShader, ERHIShaderStage::Pixel);
        const RHIPipelineLayout SkyLayout = SkyVertex != nullptr && SkyPixel != nullptr
            ? RHIPipelineLayout::FromReflection(*SkyVertex, *SkyPixel)
            : RHIPipelineLayout{};

        RHIGraphicsPipelineDesc PipelineDesc;
        PipelineDesc.VertexShader = Shaders.GetVertexShader(SkyShader);
        PipelineDesc.PixelShader = Shaders.GetPixelShader(SkyShader);
        PipelineDesc.Layout = &SkyLayout;
        PipelineDesc.RenderTargetFormat = GetSceneColorFormat();
        PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
        PipelineDesc.bEnableDepth = false;
        PipelineDesc.DepthFormat = ERHIFormat::D32_Float;
        SkyPipeline = Device->CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
        if (SkyPipeline == nullptr)
        {
            return false;
        }

        SkyConstantBuffer = Device->CreateConstantBuffer(256, ErrorMessage);
        return SkyConstantBuffer != nullptr;
    }

    void Renderer::Finalize()
    {
        CommandList.reset();
        SkyPipeline.reset();
        SkyConstantBuffer.reset();
        EnvironmentDescriptor = 0;
        EnvironmentSampler = 0;
        MeshBatches.clear();
        GpuSamplers.clear();
        GpuTextures.clear();
        TextureDescriptors.clear();
        DepthBuffer.reset();
        ConstantBuffer.reset();
        ConstantCapacity = 0;
        SwapChain = nullptr;
        ShaderLibrary = nullptr;
        Device = nullptr;
    }

    void Renderer::SetCameraMoveSpeed(
        float ForwardSpeed,
        float RightSpeed,
        float UpSpeed,
        float DownSpeed)
    {
        ViewCamera.SetMoveSpeed(ForwardSpeed, RightSpeed, UpSpeed, DownSpeed);
    }

    void Renderer::UpdateCamera(
        float DeltaTime,
        float Forward,
        float Right,
        float Up,
        float Yaw,
        float Pitch)
    {
        ViewCamera.AddLook(Yaw, Pitch);
        ViewCamera.Move(DeltaTime, Forward, Right, Up);
    }

    void Renderer::Sync(const Scene& InScene)
    {
        SyncRenderProxy(InScene, SceneProxy);
    }
}
