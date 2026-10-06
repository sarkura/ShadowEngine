#include "Framework/Render/Public/Renderer.h"

#include "Framework/Asset/Public/Asset.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/Material/Public/MaterialInstance.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace ShadowEngine
{
    namespace
    {
        constexpr char VertexEntryPoint[] = "VertexMain";
        constexpr char PixelEntryPoint[] = "PixelMain";
        constexpr uint32 ConstantAlignment = 512;

        struct GpuObjectConstants
        {
            float ViewProjection[16];
            float World[16];
            float NormalMatrix[16];
            float CameraPosition[4];
            float LightDirection[4];
            float LightColor[4];
            float BaseColor[4];
            float SpecularColor[4];
            float Roughness[4];
        };

        static_assert(sizeof(GpuObjectConstants) <= ConstantAlignment);

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
        const RenderProxy& InProxy,
        const RHIColor& InClearColor,
        std::string* ErrorMessage)
    {
        Device = &InDevice;
        SwapChain = &InSwapChain;
        ClearColor = InClearColor;

        std::vector<const MeshAsset*> UniqueMeshes;
        for (const MeshRenderProxy& Proxy : InProxy.Meshes)
        {
            const MeshAsset* Mesh = Proxy.Mesh;
            if (Mesh == nullptr)
            {
                continue;
            }

            bool bUploaded = false;
            for (const MeshAsset* Existing : UniqueMeshes)
            {
                if (Existing == Mesh)
                {
                    bUploaded = true;
                    break;
                }
            }
            if (!bUploaded)
            {
                UniqueMeshes.push_back(Mesh);
            }
        }

        if (UniqueMeshes.empty())
        {
            SetErrorMessage(ErrorMessage, "Scene has no mesh asset");
            return false;
        }

        for (const MeshAsset* Mesh : UniqueMeshes)
        {
            if (!UploadMesh(Shaders, *Mesh, ErrorMessage))
            {
                return false;
            }
        }

        uint32 DrawCount = 0;
        for (const MeshRenderProxy& Proxy : InProxy.Meshes)
        {
            if (Proxy.Mesh != nullptr)
            {
                DrawCount += static_cast<uint32>(Proxy.Mesh->GetSections().size());
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

    bool Renderer::UploadMesh(ShaderManager& Shaders, const MeshAsset& Mesh, std::string* ErrorMessage)
    {
        if (Mesh.GetSections().empty())
        {
            SetErrorMessage(ErrorMessage, "Mesh has no sections: " + Mesh.GetPath().generic_string());
            return false;
        }

        for (const MeshSection& Section : Mesh.GetSections())
        {
            const MaterialInstance* Material = Section.Material;
            if (Material == nullptr)
            {
                SetErrorMessage(ErrorMessage, "Mesh section has no material: " + Mesh.GetPath().generic_string());
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
            Batch.Asset = &Mesh;
            Batch.VertexShader = Shaders.LoadShader(
                *Device,
                Material->GetMaterial().GetShaderPath(),
                VertexEntryPoint,
                ERHIShaderStage::Vertex,
                ErrorMessage);
            if (Batch.VertexShader == nullptr)
            {
                return false;
            }

            Batch.PixelShader = Shaders.LoadShader(
                *Device,
                Material->GetMaterial().GetShaderPath(),
                PixelEntryPoint,
                ERHIShaderStage::Pixel,
                ErrorMessage);
            if (Batch.PixelShader == nullptr)
            {
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
            };

            RHIGraphicsPipelineDesc PipelineDesc;
            PipelineDesc.VertexShader = Batch.VertexShader.get();
            PipelineDesc.PixelShader = Batch.PixelShader.get();
            PipelineDesc.RenderTargetFormat = SwapChain->GetFormat();
            PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
            PipelineDesc.InputLayout = InputLayout;
            PipelineDesc.bEnableDepth = true;
            Batch.Pipeline = Device->CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
            if (Batch.Pipeline == nullptr)
            {
                return false;
            }

            MeshBatches.push_back(std::move(Batch));
        }

        return true;
    }

    void Renderer::Finalize()
    {
        CommandList.reset();
        MeshBatches.clear();
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

    bool Renderer::RenderFrame(const RenderProxy& InProxy)
    {
        if (Device == nullptr || SwapChain == nullptr || CommandList == nullptr || ConstantBuffer == nullptr ||
            DepthBuffer == nullptr || MeshBatches.empty())
        {
            return false;
        }

        const float Width = static_cast<float>(SwapChain->GetWidth());
        const float Height = static_cast<float>(SwapChain->GetHeight());
        const float Aspect = Height > 0.0F ? Width / Height : 1.0F;
        const glm::mat4 View = ViewCamera.ViewMatrix();
        const glm::mat4 Projection = glm::perspectiveRH_ZO(glm::radians(45.0F), Aspect, 0.1F, 100.0F);
        const glm::mat4 ViewProjection = Projection * View;
        const glm::vec3 CameraPosition = ViewCamera.GetPosition();

        if (InProxy.Meshes.size() > ConstantCapacity)
        {
            Log::Error("Scene has more objects than the constant buffer can hold");
            return false;
        }

        struct Draw
        {
            const MeshBatch* Batch = nullptr;
            const MeshRenderProxy* Proxy = nullptr;
            const MaterialParameter* Parameter = nullptr;
        };

        std::vector<Draw> Draws;
        for (const MeshRenderProxy& Proxy : InProxy.Meshes)
        {
            if (Proxy.Mesh == nullptr)
            {
                Log::Error("Render proxy has no mesh");
                return false;
            }

            std::vector<const MeshBatch*> SectionBatches;
            for (const MeshBatch& Batch : MeshBatches)
            {
                if (Batch.Asset == Proxy.Mesh)
                {
                    SectionBatches.push_back(&Batch);
                }
            }

            const std::vector<MeshSection>& Sections = Proxy.Mesh->GetSections();
            if (SectionBatches.size() != Sections.size())
            {
                Log::Error("Render proxy mesh was not uploaded");
                return false;
            }

            for (size_t SectionIndex = 0; SectionIndex < Sections.size(); ++SectionIndex)
            {
                const MaterialInstance* Material = Sections[SectionIndex].Material;
                if (Material == nullptr)
                {
                    Log::Error("Render proxy section has no material");
                    return false;
                }

                Draw Item;
                Item.Batch = SectionBatches[SectionIndex];
                Item.Proxy = &Proxy;
                Item.Parameter = &Material->GetParameter();
                Draws.push_back(Item);
            }
        }

        if (Draws.size() > ConstantCapacity)
        {
            Log::Error("Scene has more objects than the constant buffer can hold");
            return false;
        }

        DirectLightRenderProxy Light{};
        if (!InProxy.DirectLights.empty())
        {
            Light = InProxy.DirectLights.front();
        }

        std::vector<uint8> Constants(Draws.size() * ConstantAlignment);
        for (size_t Index = 0; Index < Draws.size(); ++Index)
        {
            const MaterialParameter& Parameter = *Draws[Index].Parameter;
            GpuObjectConstants Slot{};
            std::memcpy(Slot.ViewProjection, glm::value_ptr(ViewProjection), sizeof(Slot.ViewProjection));
            std::memcpy(Slot.World, Draws[Index].Proxy->World, sizeof(Slot.World));
            std::memcpy(Slot.NormalMatrix, Draws[Index].Proxy->NormalMatrix, sizeof(Slot.NormalMatrix));
            Slot.CameraPosition[0] = CameraPosition.x;
            Slot.CameraPosition[1] = CameraPosition.y;
            Slot.CameraPosition[2] = CameraPosition.z;
            Slot.LightDirection[0] = Light.Direction[0];
            Slot.LightDirection[1] = Light.Direction[1];
            Slot.LightDirection[2] = Light.Direction[2];
            Slot.LightColor[0] = Light.Color[0];
            Slot.LightColor[1] = Light.Color[1];
            Slot.LightColor[2] = Light.Color[2];
            Slot.LightColor[3] = Light.Intensity;
            Slot.BaseColor[0] = Parameter.BaseColor[0];
            Slot.BaseColor[1] = Parameter.BaseColor[1];
            Slot.BaseColor[2] = Parameter.BaseColor[2];
            Slot.SpecularColor[0] = Parameter.SpecularColor[0];
            Slot.SpecularColor[1] = Parameter.SpecularColor[1];
            Slot.SpecularColor[2] = Parameter.SpecularColor[2];
            Slot.Roughness[0] = Parameter.Roughness;
            std::memcpy(Constants.data() + Index * ConstantAlignment, &Slot, sizeof(Slot));
        }
        if (!Constants.empty() && !ConstantBuffer->Update(0, Constants))
        {
            Log::Error("Failed to write scene constants");
            return false;
        }

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
