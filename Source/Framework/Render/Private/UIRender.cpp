#include "Framework/Render/Public/UIRender.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIDefinitions.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "Framework/RHI/Public/RHIPipelineLayout.h"
#include "Framework/RHI/Public/RHISampler.h"
#include "Framework/RHI/Public/RHITexture.h"
#include "Framework/Shader/Public/ShaderManager.h"
#include "Framework/UI/Public/UIManager.h"

#include "ThirdPartyAdapter/UI/Public/ImGuiAdapter.h"

#include <cstddef>
#include <cstring>
#include <span>
#include <vector>

namespace ShadowEngine
{
    namespace
    {
        constexpr uint32 ConstantAlignment = 256;
        constexpr uint32 ConstantCapacity = 256;

        struct ImGuiGpuConstants
        {
            float Projection[16];
            uint32 Texture = 0;
            uint32 Sampler = 0;
            uint32 Pad[2] = {};
        };

        static_assert(sizeof(ImGuiGpuConstants) == 80);
        static_assert(offsetof(ImGuiGpuConstants, Texture) == 64);
        static_assert(sizeof(ImGuiGpuConstants) <= ConstantAlignment);
        static_assert(sizeof(ImDrawVert) == 20);

        void WriteProjection(float Projection[16], const ImDrawData& DrawData)
        {
            const float Left = DrawData.DisplayPos.x;
            const float Right = DrawData.DisplayPos.x + DrawData.DisplaySize.x;
            const float Top = DrawData.DisplayPos.y;
            const float Bottom = DrawData.DisplayPos.y + DrawData.DisplaySize.y;
            const float Matrix[16] = {
                2.0F / (Right - Left), 0.0F, 0.0F, 0.0F,
                0.0F, 2.0F / (Top - Bottom), 0.0F, 0.0F,
                0.0F, 0.0F, 0.5F, 0.0F,
                (Right + Left) / (Left - Right), (Top + Bottom) / (Bottom - Top), 0.5F, 1.0F};
            std::memcpy(Projection, Matrix, sizeof(Matrix));
        }
    }

    bool UIRender::Ensure(RHIDevice& InDevice, ShaderManager& Shaders, std::string* ErrorMessage)
    {
        Device = &InDevice;
        if (Pipeline != nullptr)
        {
            return true;
        }

        LinearSampler = InDevice.CreateClampSampler(ErrorMessage);
        PointSampler = InDevice.CreatePointClampSampler(ErrorMessage);
        ConstantBuffer = InDevice.CreateConstantBuffer(ConstantAlignment * ConstantCapacity, ErrorMessage);
        if (LinearSampler == nullptr || PointSampler == nullptr || ConstantBuffer == nullptr ||
            !CreatePipeline(Shaders, ErrorMessage))
        {
            Pipeline.reset();
            ConstantBuffer.reset();
            LinearSampler.reset();
            PointSampler.reset();
            if (ErrorMessage != nullptr && ErrorMessage->empty())
            {
                SetErrorMessage(ErrorMessage, "Interface pipeline initialization failed");
            }
            return false;
        }

        return true;
    }

    bool UIRender::CreatePipeline(ShaderManager& Shaders, std::string* ErrorMessage)
    {
        if (Device == nullptr)
        {
            SetErrorMessage(ErrorMessage, "Interface pipeline requires a device");
            return false;
        }

        const ShaderHandle Shader = Shaders.Register("ImGui/ImGui.slang", "ImGui/ImGui.slang", {}, {});
        if (!Shader.IsValid() || !Shaders.LoadShaders(*Device, Shader, ErrorMessage))
        {
            return false;
        }

        const ShaderReflection* VertexReflection = Shaders.GetReflection(Shader, ERHIShaderStage::Vertex);
        const ShaderReflection* PixelReflection = Shaders.GetReflection(Shader, ERHIShaderStage::Pixel);
        const RHIPipelineLayout Layout = VertexReflection != nullptr && PixelReflection != nullptr
            ? RHIPipelineLayout::FromReflection(*VertexReflection, *PixelReflection)
            : RHIPipelineLayout{};
        const RHIInputElement InputLayout[] = {
            {"POSITION", 0, 0, ERHIVertexFormat::Float32x2},
            {"TEXCOORD", 0, 8, ERHIVertexFormat::Float32x2},
            {"COLOR", 0, 16, ERHIVertexFormat::UNorm8x4},
        };

        RHIGraphicsPipelineDesc PipelineDesc;
        PipelineDesc.VertexShader = Shaders.GetVertexShader(Shader);
        PipelineDesc.PixelShader = Shaders.GetPixelShader(Shader);
        PipelineDesc.Layout = &Layout;
        PipelineDesc.RenderTargetFormats[0] = ERHIFormat::R8G8B8A8_UNorm;
        PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
        PipelineDesc.InputLayout = InputLayout;
        PipelineDesc.bEnableDepth = false;
        PipelineDesc.bAlphaBlend = true;
        PipelineDesc.Cull = ERHICullMode::None;
        Pipeline = Device->CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
        return Pipeline != nullptr;
    }

    int UIRender::FindTexture(const void* Source) const
    {
        for (int Index = 0; Index < static_cast<int>(Textures.size()); ++Index)
        {
            if (Textures[static_cast<size_t>(Index)].Source == Source)
            {
                return Index;
            }
        }
        return -1;
    }

    void UIRender::UpdateTextures()
    {
        if (Device == nullptr || !ImGuiAdapter::HasContext())
        {
            return;
        }

        ImDrawData* DrawData = ImGuiAdapter::GetDrawData();
        if (DrawData == nullptr || DrawData->Textures == nullptr)
        {
            return;
        }

        for (ImTextureData* TextureData : *DrawData->Textures)
        {
            if (TextureData == nullptr || TextureData->Status == ImTextureStatus_OK)
            {
                continue;
            }

            if (TextureData->Status == ImTextureStatus_WantDestroy)
            {
                const int Found = FindTexture(TextureData);
                if (Found >= 0)
                {
                    Textures.erase(Textures.begin() + Found);
                }
                TextureData->SetTexID(ImTextureID_Invalid);
                TextureData->SetStatus(ImTextureStatus_Destroyed);
                continue;
            }

            if (TextureData->Status != ImTextureStatus_WantCreate && TextureData->Status != ImTextureStatus_WantUpdates)
            {
                continue;
            }
            if (TextureData->Format != ImTextureFormat_RGBA32 || TextureData->Pixels == nullptr ||
                TextureData->Width <= 0 || TextureData->Height <= 0)
            {
                Log::Error("Interface texture is not RGBA8");
                continue;
            }

            int Found = FindTexture(TextureData);
            if (Found < 0)
            {
                FontTexture Created;
                Created.Source = TextureData;
                Textures.push_back(std::move(Created));
                Found = static_cast<int>(Textures.size()) - 1;
            }

            FontTexture& Slot = Textures[static_cast<size_t>(Found)];
            const RHITextureMipDesc Mip{
                static_cast<uint32>(TextureData->Width),
                static_cast<uint32>(TextureData->Height),
                TextureData->Pixels,
                static_cast<uint32>(TextureData->GetSizeInBytes())};
            const RHITextureDesc Desc{ERHIFormat::R8G8B8A8_UNorm, std::span<const RHITextureMipDesc>(&Mip, 1)};
            std::unique_ptr<RHITexture> Uploaded = Device->CreateTexture(Desc, nullptr);
            if (Uploaded == nullptr)
            {
                Log::Error("Failed to upload an interface texture");
                continue;
            }

            uint32 Descriptor = Slot.Descriptor;
            const bool bViewReady = Descriptor == 0xffffffff
                ? Device->CreateShaderResourceView(*Uploaded, Descriptor, nullptr)
                : Device->UpdateShaderResourceView(*Uploaded, Descriptor, nullptr);
            if (!bViewReady)
            {
                Log::Error("Failed to bind an interface texture");
                continue;
            }

            Slot.Texture = std::move(Uploaded);
            Slot.Descriptor = Descriptor;
            TextureData->SetTexID(static_cast<ImTextureID>(Descriptor + 1));
            TextureData->SetStatus(ImTextureStatus_OK);
        }
    }

    bool UIRender::UploadGeometry(const void* Vertices, uint32 VertexCount, const void* Indices, uint32 IndexCount)
    {
        if (Device == nullptr || Vertices == nullptr || Indices == nullptr || VertexCount == 0 || IndexCount == 0)
        {
            return false;
        }

        constexpr uint32 VertexStride = sizeof(ImDrawVert);
        constexpr uint32 IndexStride = sizeof(ImDrawIdx);
        constexpr ERHIIndexFormat IndexFormat =
            sizeof(ImDrawIdx) == sizeof(uint16) ? ERHIIndexFormat::Uint16 : ERHIIndexFormat::Uint32;
        const uint32 VertexBytes = VertexCount * VertexStride;
        const uint32 IndexBytes = IndexCount * IndexStride;

        if (VertexBuffer == nullptr || VertexCapacity < VertexCount)
        {
            VertexCapacity = VertexCount + 5000;
            std::vector<uint8> Storage(static_cast<size_t>(VertexCapacity) * VertexStride);
            std::memcpy(Storage.data(), Vertices, VertexBytes);
            RHIBufferDesc Desc;
            Desc.Stride = VertexStride;
            Desc.Data = Storage;
            VertexBuffer = Device->CreateVertexBuffer(Desc, nullptr);
            if (VertexBuffer == nullptr)
            {
                VertexCapacity = 0;
                return false;
            }
        }
        else if (!VertexBuffer->Update(0, std::span<const uint8>(static_cast<const uint8*>(Vertices), VertexBytes)))
        {
            return false;
        }

        if (IndexBuffer == nullptr || IndexCapacity < IndexCount)
        {
            IndexCapacity = IndexCount + 10000;
            std::vector<uint8> Storage(static_cast<size_t>(IndexCapacity) * IndexStride);
            std::memcpy(Storage.data(), Indices, IndexBytes);
            RHIBufferDesc Desc;
            Desc.Stride = IndexStride;
            Desc.Data = Storage;
            IndexBuffer = Device->CreateIndexBuffer(Desc, IndexFormat, nullptr);
            if (IndexBuffer == nullptr)
            {
                IndexCapacity = 0;
                return false;
            }
        }
        else if (!IndexBuffer->Update(0, std::span<const uint8>(static_cast<const uint8*>(Indices), IndexBytes)))
        {
            return false;
        }

        return true;
    }

    void UIRender::Draw(RHICommandList& Commands, RHITexture& Target)
    {
        if (!UIManager::Get().IsActive() || Device == nullptr || Pipeline == nullptr || ConstantBuffer == nullptr ||
            LinearSampler == nullptr || PointSampler == nullptr || !ImGuiAdapter::HasContext())
        {
            return;
        }

        ImDrawData* DrawData = ImGuiAdapter::GetDrawData();
        if (DrawData == nullptr)
        {
            return;
        }

        UpdateTextures();
        const float ViewportWidth = DrawData->DisplaySize.x * DrawData->FramebufferScale.x;
        const float ViewportHeight = DrawData->DisplaySize.y * DrawData->FramebufferScale.y;
        if (ViewportWidth <= 0.0F || ViewportHeight <= 0.0F || DrawData->TotalVtxCount <= 0 || DrawData->TotalIdxCount <= 0)
        {
            return;
        }

        std::vector<ImDrawVert> Vertices;
        std::vector<ImDrawIdx> Indices;
        Vertices.reserve(static_cast<size_t>(DrawData->TotalVtxCount));
        Indices.reserve(static_cast<size_t>(DrawData->TotalIdxCount));
        for (int ListIndex = 0; ListIndex < DrawData->CmdLists.Size; ++ListIndex)
        {
            const ImDrawList* List = DrawData->CmdLists[ListIndex];
            Vertices.insert(Vertices.end(), List->VtxBuffer.Data, List->VtxBuffer.Data + List->VtxBuffer.Size);
            Indices.insert(Indices.end(), List->IdxBuffer.Data, List->IdxBuffer.Data + List->IdxBuffer.Size);
        }
        if (!UploadGeometry(Vertices.data(), static_cast<uint32>(Vertices.size()), Indices.data(), static_cast<uint32>(Indices.size())))
        {
            Log::Error("Failed to upload interface geometry");
            return;
        }

        float Projection[16] = {};
        WriteProjection(Projection, *DrawData);
        const ImVec2 ClipOffset = DrawData->DisplayPos;
        const ImVec2 ClipScale = DrawData->FramebufferScale;
        const ImGuiPlatformIO& Platform = ImGuiAdapter::GetPlatformIO();
        const uint32 LinearIndex = LinearSampler->GetDescriptorIndex();
        const uint32 PointIndex = PointSampler->GetDescriptorIndex();
        uint32 SamplerIndex = LinearIndex;

        Commands.BeginRenderPass(Target, nullptr, {}, false, false);
        const RHIViewport Viewport{0.0F, 0.0F, ViewportWidth, ViewportHeight, 0.0F, 1.0F};
        Commands.SetViewport(Viewport);
        Commands.SetPipeline(*Pipeline);
        Commands.BindShaderResources();
        Commands.SetVertexBuffer(*VertexBuffer);
        Commands.SetIndexBuffer(*IndexBuffer);

        uint32 Slot = 0;
        uint32 VertexBase = 0;
        uint32 IndexBase = 0;
        for (int ListIndex = 0; ListIndex < DrawData->CmdLists.Size; ++ListIndex)
        {
            const ImDrawList* List = DrawData->CmdLists[ListIndex];
            for (int CommandIndex = 0; CommandIndex < List->CmdBuffer.Size; ++CommandIndex)
            {
                const ImDrawCmd& Command = List->CmdBuffer[CommandIndex];
                if (Command.UserCallback != nullptr)
                {
                    if (Command.UserCallback == Platform.DrawCallback_SetSamplerNearest)
                    {
                        SamplerIndex = PointIndex;
                    }
                    else if (Command.UserCallback == Platform.DrawCallback_SetSamplerLinear)
                    {
                        SamplerIndex = LinearIndex;
                    }
                    else if (Command.UserCallback == Platform.DrawCallback_ResetRenderState)
                    {
                        SamplerIndex = LinearIndex;
                        Commands.SetViewport(Viewport);
                        Commands.SetPipeline(*Pipeline);
                        Commands.SetVertexBuffer(*VertexBuffer);
                        Commands.SetIndexBuffer(*IndexBuffer);
                    }
                    else
                    {
                        Command.UserCallback(List, &Command);
                    }
                    continue;
                }

                const ImVec2 ClipMin(
                    (Command.ClipRect.x - ClipOffset.x) * ClipScale.x,
                    (Command.ClipRect.y - ClipOffset.y) * ClipScale.y);
                const ImVec2 ClipMax(
                    (Command.ClipRect.z - ClipOffset.x) * ClipScale.x,
                    (Command.ClipRect.w - ClipOffset.y) * ClipScale.y);
                if (ClipMax.x <= ClipMin.x || ClipMax.y <= ClipMin.y || Command.GetTexID() == ImTextureID_Invalid)
                {
                    continue;
                }
                if (Slot >= ConstantCapacity)
                {
                    Log::Error("Interface draw exceeded the constant buffer");
                    Commands.EndRenderPass();
                    return;
                }

                ImGuiGpuConstants Constants{};
                std::memcpy(Constants.Projection, Projection, sizeof(Projection));
                Constants.Texture = static_cast<uint32>(Command.GetTexID() - 1);
                Constants.Sampler = SamplerIndex;
                const std::span<const uint8> Bytes(reinterpret_cast<const uint8*>(&Constants), sizeof(Constants));
                if (!ConstantBuffer->Update(Slot * ConstantAlignment, Bytes))
                {
                    Log::Error("Failed to write interface constants");
                    Commands.EndRenderPass();
                    return;
                }

                Commands.SetConstantBuffer(*ConstantBuffer, Slot * ConstantAlignment);
                Commands.SetScissor({
                    static_cast<int32>(ClipMin.x),
                    static_cast<int32>(ClipMin.y),
                    static_cast<int32>(ClipMax.x),
                    static_cast<int32>(ClipMax.y)});
                Commands.DrawIndexed(Command.ElemCount, IndexBase + Command.IdxOffset, static_cast<int32>(VertexBase + Command.VtxOffset));
                ++Slot;
            }

            VertexBase += static_cast<uint32>(List->VtxBuffer.Size);
            IndexBase += static_cast<uint32>(List->IdxBuffer.Size);
        }

        Commands.EndRenderPass();
    }
}
