#include "Framework/Render/Public/Renderer.h"

#include "Framework/Asset/Public/AssetManager.h"
#include "Framework/Asset/Public/MeshAsset.h"
#include "Framework/Asset/Public/TextureAsset.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/Material/Public/Material.h"
#include "Framework/Material/Public/MaterialInstance.h"
#include "Framework/Material/Public/MaterialParameter.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/RHI/Public/RHISwapChain.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <algorithm>
#include <cstdint>
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
    }

    Renderer::~Renderer()
    {
        Finalize();
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

            RHIGraphicsPipelineDesc PipelineDesc;
            PipelineDesc.VertexShader = Batch.VertexShader;
            PipelineDesc.PixelShader = Batch.PixelShader;
            PipelineDesc.RenderTargetFormat = SwapChain->GetFormat();
            PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
            PipelineDesc.InputLayout = InputLayout;
            PipelineDesc.bEnableDepth = true;
            Batch.Pipeline = Device->CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
            if (Batch.Pipeline == nullptr)
            {
                return false;
            }

            const MaterialParameter::TextureBinding Textures = MaterialParameter::GetTextureBinding(Instance->GetParameters());
            if (!Textures.IsValid())
            {
                SetErrorMessage(ErrorMessage, "Material textures must include BaseColor, Roughness, and Normal: " + Mesh.GetPath().generic_string());
                return false;
            }

            if (Textures.HasTextures())
            {
                RHITexture* BaseColor = UploadTexture(Textures.BaseColor, ErrorMessage);
                RHITexture* Roughness = UploadTexture(Textures.Roughness, ErrorMessage);
                RHITexture* Normal = UploadTexture(Textures.Normal, ErrorMessage);
                RHISampler* Sampler = UploadSampler(Textures.Sampler, ErrorMessage);
                if (BaseColor == nullptr || Roughness == nullptr || Normal == nullptr || Sampler == nullptr)
                {
                    return false;
                }
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
        TextureDesc.Format = ERHIFormat::R8G8B8A8_UNorm;
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

    void Renderer::Finalize()
    {
        CommandList.reset();
        MeshBatches.clear();
        GpuSamplers.clear();
        GpuTextures.clear();
        TextureDescriptors.clear();
        DepthBuffer.reset();
        ConstantBuffer.reset();
        ConstantCapacity = 0;
        SwapChain = nullptr;
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

    bool Renderer::CollectDraws(std::vector<FrameDraw>& Draws) const
    {
        Draws.clear();
        for (const MeshRenderProxy& Proxy : SceneProxy.Meshes)
        {
            const MeshAsset* Mesh = Assets->ResolveMesh(Proxy.Mesh);
            if (Mesh == nullptr)
            {
                Log::Error("Render proxy has no mesh");
                return false;
            }

            const std::vector<MeshSection>& Sections = Mesh->GetSections();
            if (Proxy.Materials.size() != Sections.size())
            {
                Log::Error("Render proxy mesh was not uploaded");
                return false;
            }

            for (uint32 SectionIndex = 0; SectionIndex < static_cast<uint32>(Sections.size()); ++SectionIndex)
            {
                if (!Proxy.Materials[SectionIndex].IsValid())
                {
                    Log::Error("Render proxy section has no material");
                    return false;
                }

                const MaterialInstance* Instance = Assets->ResolveMaterialInstance(Proxy.Materials[SectionIndex]);
                const Material* BoundMaterial = Instance == nullptr ? nullptr : Assets->ResolveMaterial(Instance->GetBaseMaterial());
                if (BoundMaterial == nullptr)
                {
                    Log::Error("Render proxy material is missing");
                    return false;
                }

                if (BoundMaterial->FindShader(GetName()) == nullptr)
                {
                    continue;
                }

                const MeshBatch* Batch = nullptr;
                for (const MeshBatch& Existing : MeshBatches)
                {
                    if (Existing.Mesh.Index == Proxy.Mesh.Index && Existing.Section == SectionIndex)
                    {
                        Batch = &Existing;
                        break;
                    }
                }
                if (Batch == nullptr)
                {
                    Log::Error("Render proxy mesh was not uploaded");
                    return false;
                }

                FrameDraw Item;
                Item.Batch = Batch;
                Item.Proxy = &Proxy;
                Item.Material = Proxy.Materials[SectionIndex];
                Draws.push_back(Item);
            }
        }

        return true;
    }

    bool Renderer::SubmitDraws(const std::vector<FrameDraw>& Draws)
    {
        if (Device == nullptr || SwapChain == nullptr || CommandList == nullptr || DepthBuffer == nullptr)
        {
            return false;
        }

        if (!Draws.empty() && ConstantBuffer == nullptr)
        {
            return false;
        }

        const float Width = static_cast<float>(SwapChain->GetWidth());
        const float Height = static_cast<float>(SwapChain->GetHeight());
        CommandList->Begin();
        CommandList->BeginRenderPass(SwapChain->GetCurrentBackBuffer(), DepthBuffer.get(), ClearColor);
        CommandList->SetViewport({0.0F, 0.0F, Width, Height, 0.0F, 1.0F});
        CommandList->SetScissor({
            0,
            0,
            static_cast<int32>(SwapChain->GetWidth()),
            static_cast<int32>(SwapChain->GetHeight())});
        for (uint32 Index = 0; Index < static_cast<uint32>(Draws.size()); ++Index)
        {
            const MeshBatch& Batch = *Draws[Index].Batch;
            CommandList->SetPipeline(*Batch.Pipeline);
            CommandList->BindShaderResources();
            CommandList->SetVertexBuffer(*Batch.VertexBuffer);
            CommandList->SetIndexBuffer(*Batch.IndexBuffer);
            CommandList->SetConstantBuffer(*ConstantBuffer, Index * ConstantAlignment);
            CommandList->DrawIndexed(Batch.IndexCount);
        }
        CommandList->EndRenderPass();
        CommandList->End();

        Device->SubmitCommandList(*CommandList);
        const bool bPresented = SwapChain->Present();
        Device->WaitIdle();
        return bPresented;
    }
}
