#include "Framework/Render/Public/PostProcess.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/Render/Public/MeshBatch.h"
#include "Framework/Render/Public/RenderItem.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIDefinitions.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "Framework/RHI/Public/RHIPipelineLayout.h"
#include "Framework/RHI/Public/RHISampler.h"
#include "Framework/RHI/Public/RHITexture.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <algorithm>
#include <cstring>
#include <span>

namespace ShadowEngine
{
    bool PostProcess::Ensure(
        RHIDevice& Device,
        ShaderManager& Shaders,
        RHITexture& Depth,
        uint32 InWidth,
        uint32 InHeight,
        ERHIFormat OutputFormat,
        std::string* ErrorMessage)
    {
        static_assert(sizeof(PostGpuConstants) == 208);
        if (InWidth == 0 || InHeight == 0)
        {
            SetErrorMessage(ErrorMessage, "Post process requires a non-zero viewport");
            return false;
        }

        if (!bPipelinesReady)
        {
            if (!CreateFullscreenPipeline(Device, Shaders, "PostProcess/CameraVelocity.slang", ERHIFormat::R16G16B16A16_Float, CameraVelocityPipeline, ErrorMessage) ||
                !CreateFullscreenPipeline(Device, Shaders, "PostProcess/TAA.slang", ERHIFormat::R16G16B16A16_Float, TAAPipeline, ErrorMessage) ||
                !CreateFullscreenPipeline(Device, Shaders, "PostProcess/BloomExtract.slang", ERHIFormat::R16G16B16A16_Float, BloomExtractPipeline, ErrorMessage) ||
                !CreateFullscreenPipeline(Device, Shaders, "PostProcess/BloomDownsample.slang", ERHIFormat::R16G16B16A16_Float, BloomDownsamplePipeline, ErrorMessage) ||
                !CreateFullscreenPipeline(Device, Shaders, "PostProcess/BloomUpsample.slang", ERHIFormat::R16G16B16A16_Float, BloomUpsamplePipeline, ErrorMessage) ||
                !CreateFullscreenPipeline(Device, Shaders, "PostProcess/Tonemap.slang", OutputFormat, TonemapPipeline, ErrorMessage))
            {
                return false;
            }

            const ShaderHandle VelocityShader = Shaders.Register(
                "PostProcess/MeshVelocity.slang",
                "PostProcess/MeshVelocity.slang",
                {},
                {});
            if (!VelocityShader.IsValid() || !Shaders.LoadShaders(Device, VelocityShader, ErrorMessage))
            {
                return false;
            }

            const ShaderReflection* VertexReflection = Shaders.GetReflection(VelocityShader, ERHIShaderStage::Vertex);
            const ShaderReflection* PixelReflection = Shaders.GetReflection(VelocityShader, ERHIShaderStage::Pixel);
            const RHIPipelineLayout Layout = VertexReflection != nullptr && PixelReflection != nullptr
                ? RHIPipelineLayout::FromReflection(*VertexReflection, *PixelReflection)
                : RHIPipelineLayout{};
            constexpr RHIInputElement InputLayout[] = {
                {"POSITION", 0, 0, ERHIVertexFormat::Float32x3},
            };
            RHIGraphicsPipelineDesc PipelineDesc;
            PipelineDesc.VertexShader = Shaders.GetVertexShader(VelocityShader);
            PipelineDesc.PixelShader = Shaders.GetPixelShader(VelocityShader);
            PipelineDesc.Layout = &Layout;
            PipelineDesc.RenderTargetFormat = ERHIFormat::R16G16B16A16_Float;
            PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
            PipelineDesc.InputLayout = InputLayout;
            PipelineDesc.bEnableDepth = true;
            PipelineDesc.bWriteDepth = false;
            PipelineDesc.bDepthEqual = true;
            PipelineDesc.DepthFormat = ERHIFormat::D32_Float;
            MeshVelocityPipeline = Device.CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
            if (MeshVelocityPipeline == nullptr)
            {
                return false;
            }

            ClampSampler = Device.CreateClampSampler(ErrorMessage);
            ConstantBuffer = Device.CreateConstantBuffer(256 * 16, ErrorMessage);
            if (ClampSampler == nullptr || ConstantBuffer == nullptr)
            {
                return false;
            }

            bPipelinesReady = true;
        }

        const bool bResize = Width != InWidth || Height != InHeight || SceneColor.Texture == nullptr;
        if (bResize)
        {
            if (!CreateTarget(Device, SceneColor, InWidth, InHeight, ErrorMessage) ||
                !CreateTarget(Device, Motion, InWidth, InHeight, ErrorMessage) ||
                !CreateTarget(Device, History[0], InWidth, InHeight, ErrorMessage) ||
                !CreateTarget(Device, History[1], InWidth, InHeight, ErrorMessage))
            {
                return false;
            }

            uint32 LevelWidth = std::max(1u, InWidth / 2);
            uint32 LevelHeight = std::max(1u, InHeight / 2);
            for (Target& Level : BloomDown)
            {
                if (!CreateTarget(Device, Level, LevelWidth, LevelHeight, ErrorMessage))
                {
                    return false;
                }

                LevelWidth = std::max(1u, LevelWidth / 2);
                LevelHeight = std::max(1u, LevelHeight / 2);
            }

            LevelWidth = std::max(1u, InWidth / 2);
            LevelHeight = std::max(1u, InHeight / 2);
            for (Target& Level : BloomUp)
            {
                if (!CreateTarget(Device, Level, LevelWidth, LevelHeight, ErrorMessage))
                {
                    return false;
                }

                LevelWidth = std::max(1u, LevelWidth / 2);
                LevelHeight = std::max(1u, LevelHeight / 2);
            }

            Width = InWidth;
            Height = InHeight;
            bHistoryValid = false;
            HistoryIndex = 0;
        }

        if (DepthTexture != &Depth || DepthDescriptor == 0xffffffff)
        {
            if (DepthDescriptor == 0xffffffff)
            {
                if (!Device.CreateShaderResourceView(Depth, DepthDescriptor, ErrorMessage))
                {
                    return false;
                }
            }
            else if (!Device.UpdateShaderResourceView(Depth, DepthDescriptor, ErrorMessage))
            {
                return false;
            }

            DepthTexture = &Depth;
        }

        return SceneColor.Texture != nullptr;
    }

    RHITexture* PostProcess::GetSceneColor() const
    {
        return SceneColor.Texture.get();
    }

    bool PostProcess::CreateTarget(
        RHIDevice& Device,
        Target& Out,
        uint32 InWidth,
        uint32 InHeight,
        std::string* ErrorMessage)
    {
        Out.Texture = Device.CreateColorTarget(InWidth, InHeight, ERHIFormat::R16G16B16A16_Float, ErrorMessage);
        if (Out.Texture == nullptr)
        {
            return false;
        }

        const bool bCreated = Out.Descriptor == 0xffffffff
            ? Device.CreateShaderResourceView(*Out.Texture, Out.Descriptor, ErrorMessage)
            : Device.UpdateShaderResourceView(*Out.Texture, Out.Descriptor, ErrorMessage);
        if (!bCreated)
        {
            return false;
        }

        Out.Width = InWidth;
        Out.Height = InHeight;
        return true;
    }

    bool PostProcess::CreateFullscreenPipeline(
        RHIDevice& Device,
        ShaderManager& Shaders,
        const char* PixelPath,
        ERHIFormat Format,
        std::unique_ptr<RHIPipeline>& Out,
        std::string* ErrorMessage)
    {
        const ShaderHandle Shader = Shaders.Register("PostProcess/Fullscreen.slang", PixelPath, {}, {});
        if (!Shader.IsValid() || !Shaders.LoadShaders(Device, Shader, ErrorMessage))
        {
            return false;
        }

        const ShaderReflection* VertexReflection = Shaders.GetReflection(Shader, ERHIShaderStage::Vertex);
        const ShaderReflection* PixelReflection = Shaders.GetReflection(Shader, ERHIShaderStage::Pixel);
        const RHIPipelineLayout Layout = VertexReflection != nullptr && PixelReflection != nullptr
            ? RHIPipelineLayout::FromReflection(*VertexReflection, *PixelReflection)
            : RHIPipelineLayout{};
        RHIGraphicsPipelineDesc PipelineDesc;
        PipelineDesc.VertexShader = Shaders.GetVertexShader(Shader);
        PipelineDesc.PixelShader = Shaders.GetPixelShader(Shader);
        PipelineDesc.Layout = &Layout;
        PipelineDesc.RenderTargetFormat = Format;
        PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
        PipelineDesc.bEnableDepth = false;
        Out = Device.CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
        return Out != nullptr;
    }

    bool PostProcess::DrawFullscreen(
        RHICommandList& Commands,
        RHITexture& TargetTexture,
        RHIPipeline& Pipeline,
        uint32 TargetWidth,
        uint32 TargetHeight,
        const PostGpuConstants& Constants)
    {
        const std::span<const uint8> Bytes(reinterpret_cast<const uint8*>(&Constants), sizeof(Constants));
        if (ConstantBuffer == nullptr || ConstantSlot >= 16 || !ConstantBuffer->Update(ConstantSlot * 256, Bytes))
        {
            Log::Error("Failed to write post constants");
            return false;
        }

        const uint32 Offset = ConstantSlot * 256;
        ++ConstantSlot;
        Commands.BeginRenderPass(TargetTexture, nullptr, {}, true);
        Commands.SetViewport({0.0F, 0.0F, static_cast<float>(TargetWidth), static_cast<float>(TargetHeight), 0.0F, 1.0F});
        Commands.SetScissor({0, 0, static_cast<int32>(TargetWidth), static_cast<int32>(TargetHeight)});
        Commands.SetPipeline(Pipeline);
        Commands.BindShaderResources();
        Commands.SetConstantBuffer(*ConstantBuffer, Offset);
        Commands.Draw(3);
        Commands.EndRenderPass();
        return true;
    }

    bool PostProcess::Draw(
        RHICommandList& Commands,
        RHITexture& BackBuffer,
        RHITexture& Depth,
        RHIBuffer& ObjectConstants,
        const std::vector<RenderItem>& Items,
        const std::vector<MeshBatch>& Batches,
        const float InverseViewProjection[16],
        const float PreviousViewProjection[16],
        const float CameraPosition[3],
        uint32 InWidth,
        uint32 InHeight,
        uint32 ConstantAlignment)
    {
        if (SceneColor.Texture == nullptr || Motion.Texture == nullptr || CameraVelocityPipeline == nullptr ||
            MeshVelocityPipeline == nullptr || TAAPipeline == nullptr || BloomExtractPipeline == nullptr ||
            BloomDownsamplePipeline == nullptr || BloomUpsamplePipeline == nullptr || TonemapPipeline == nullptr ||
            ClampSampler == nullptr)
        {
            Log::Error("Forward post process is not ready");
            return false;
        }

        Commands.UseAsShaderResource(*SceneColor.Texture);
        Commands.UseAsShaderResource(Depth);
        ConstantSlot = 0;

        PostGpuConstants Constants{};
        std::memcpy(Constants.InverseViewProjection, InverseViewProjection, sizeof(Constants.InverseViewProjection));
        std::memcpy(Constants.PreviousViewProjection, PreviousViewProjection, sizeof(Constants.PreviousViewProjection));
        Constants.CameraPosition[0] = CameraPosition[0];
        Constants.CameraPosition[1] = CameraPosition[1];
        Constants.CameraPosition[2] = CameraPosition[2];
        Constants.Sampler = ClampSampler->GetDescriptorIndex();
        Constants.Depth = DepthDescriptor;
        Constants.Motion = Motion.Descriptor;
        Constants.DestSize[0] = static_cast<float>(InWidth);
        Constants.DestSize[1] = static_cast<float>(InHeight);
        Constants.Source = SceneColor.Descriptor;
        if (!DrawFullscreen(Commands, *Motion.Texture, *CameraVelocityPipeline, InWidth, InHeight, Constants))
        {
            return false;
        }

        Commands.BeginRenderPass(*Motion.Texture, &Depth, {}, false, false);
        Commands.SetViewport({0.0F, 0.0F, static_cast<float>(InWidth), static_cast<float>(InHeight), 0.0F, 1.0F});
        Commands.SetScissor({0, 0, static_cast<int32>(InWidth), static_cast<int32>(InHeight)});
        Commands.SetPipeline(*MeshVelocityPipeline);
        Commands.BindShaderResources();
        for (uint32 Index = 0; Index < static_cast<uint32>(Items.size()); ++Index)
        {
            const MeshBatch* Batch = FindMeshBatch(Batches, Items[Index].Mesh, Items[Index].Section);
            if (Batch == nullptr || Batch->VertexBuffer == nullptr || Batch->IndexBuffer == nullptr)
            {
                Log::Error("Forward velocity is missing a mesh");
                Commands.EndRenderPass();
                return false;
            }

            Commands.SetVertexBuffer(*Batch->VertexBuffer);
            Commands.SetIndexBuffer(*Batch->IndexBuffer);
            Commands.SetConstantBuffer(ObjectConstants, Index * ConstantAlignment);
            Commands.DrawIndexed(Batch->IndexCount);
        }
        Commands.EndRenderPass();
        Commands.UseAsShaderResource(*Motion.Texture);

        const uint32 WriteIndex = HistoryIndex ^ 1u;
        Constants.Source = SceneColor.Descriptor;
        Constants.Secondary = History[HistoryIndex].Descriptor;
        Constants.CurrentWeight = bHistoryValid ? 0.1F : 1.0F;
        Constants.DestSize[0] = static_cast<float>(InWidth);
        Constants.DestSize[1] = static_cast<float>(InHeight);
        if (!DrawFullscreen(Commands, *History[WriteIndex].Texture, *TAAPipeline, InWidth, InHeight, Constants))
        {
            return false;
        }
        Commands.UseAsShaderResource(*History[WriteIndex].Texture);

        Constants.Source = History[WriteIndex].Descriptor;
        Constants.Threshold = 1.0F;
        Constants.DestSize[0] = static_cast<float>(BloomDown[0].Width);
        Constants.DestSize[1] = static_cast<float>(BloomDown[0].Height);
        Constants.SourceTexel[0] = 1.0F / static_cast<float>(InWidth);
        Constants.SourceTexel[1] = 1.0F / static_cast<float>(InHeight);
        if (!DrawFullscreen(Commands, *BloomDown[0].Texture, *BloomExtractPipeline, BloomDown[0].Width, BloomDown[0].Height, Constants))
        {
            return false;
        }
        Commands.UseAsShaderResource(*BloomDown[0].Texture);

        for (uint32 Level = 0; Level < 3; ++Level)
        {
            Constants.Source = BloomDown[Level].Descriptor;
            Constants.DestSize[0] = static_cast<float>(BloomDown[Level + 1].Width);
            Constants.DestSize[1] = static_cast<float>(BloomDown[Level + 1].Height);
            Constants.SourceTexel[0] = 1.0F / static_cast<float>(BloomDown[Level].Width);
            Constants.SourceTexel[1] = 1.0F / static_cast<float>(BloomDown[Level].Height);
            if (!DrawFullscreen(
                    Commands,
                    *BloomDown[Level + 1].Texture,
                    *BloomDownsamplePipeline,
                    BloomDown[Level + 1].Width,
                    BloomDown[Level + 1].Height,
                    Constants))
            {
                return false;
            }

            Commands.UseAsShaderResource(*BloomDown[Level + 1].Texture);
        }

        for (int32 Level = 2; Level >= 0; --Level)
        {
            const uint32 SourceDescriptor = Level == 2 ? BloomDown[3].Descriptor : BloomUp[Level + 1].Descriptor;
            Constants.Source = SourceDescriptor;
            Constants.Secondary = BloomDown[Level].Descriptor;
            Constants.DestSize[0] = static_cast<float>(BloomUp[Level].Width);
            Constants.DestSize[1] = static_cast<float>(BloomUp[Level].Height);
            if (!DrawFullscreen(
                    Commands,
                    *BloomUp[Level].Texture,
                    *BloomUpsamplePipeline,
                    BloomUp[Level].Width,
                    BloomUp[Level].Height,
                    Constants))
            {
                return false;
            }

            Commands.UseAsShaderResource(*BloomUp[Level].Texture);
        }

        Constants.Source = History[WriteIndex].Descriptor;
        Constants.Secondary = BloomUp[0].Descriptor;
        Constants.Strength = 0.08F;
        Constants.CurrentWeight = 1.0F;
        Constants.DestSize[0] = static_cast<float>(InWidth);
        Constants.DestSize[1] = static_cast<float>(InHeight);
        if (!DrawFullscreen(Commands, BackBuffer, *TonemapPipeline, InWidth, InHeight, Constants))
        {
            return false;
        }

        HistoryIndex = WriteIndex;
        bHistoryValid = true;
        return true;
    }
}
