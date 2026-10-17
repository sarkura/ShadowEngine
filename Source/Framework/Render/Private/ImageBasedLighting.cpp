#include "Framework/Render/Public/ImageBasedLighting.h"

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

#include <cstring>
#include <span>

namespace ShadowEngine
{
    bool ImageBasedLighting::Ensure(
        RHIDevice& Device,
        ShaderManager& Shaders,
        RHICommandList& Commands,
        uint32 EnvironmentDescriptor,
        uint32 EnvironmentSampler,
        std::string* ErrorMessage)
    {
        static_assert(sizeof(IblGpuConstants) == 32);
        if (bReady)
        {
            return true;
        }

        if (!bPipelinesReady)
        {
            if (!CreateFullscreenPipeline(Device, Shaders, "ImageBasedLighting/Irradiance.slang", IrradiancePipeline, ErrorMessage) ||
                !CreateFullscreenPipeline(Device, Shaders, "ImageBasedLighting/Prefilter.slang", PrefilterPipeline, ErrorMessage) ||
                !CreateFullscreenPipeline(Device, Shaders, "ImageBasedLighting/BRDFLUT.slang", BrdfPipeline, ErrorMessage))
            {
                return false;
            }

            ClampSampler = Device.CreateClampSampler(ErrorMessage);
            ConstantBuffer = Device.CreateConstantBuffer(256 * 8, ErrorMessage);
            if (ClampSampler == nullptr || ConstantBuffer == nullptr)
            {
                return false;
            }

            bPipelinesReady = true;
        }

        if (Irradiance.Texture == nullptr &&
            (!CreateTarget(Device, Irradiance, 128, 64, ErrorMessage) ||
             !CreateTarget(Device, Prefiltered[0], 256, 128, ErrorMessage) ||
             !CreateTarget(Device, Prefiltered[1], 128, 64, ErrorMessage) ||
             !CreateTarget(Device, Prefiltered[2], 64, 32, ErrorMessage) ||
             !CreateTarget(Device, Prefiltered[3], 32, 16, ErrorMessage) ||
             !CreateTarget(Device, BrdfLut, 256, 256, ErrorMessage)))
        {
            return false;
        }

        Commands.Begin();
        ConstantSlot = 0;
        const float Roughness[4] = {0.0F, 1.0F / 3.0F, 2.0F / 3.0F, 1.0F};
        bool bDrawn = Draw(
            Commands,
            *BrdfLut.Texture,
            *BrdfPipeline,
            BrdfLut.Width,
            BrdfLut.Height,
            0.0F,
            EnvironmentDescriptor,
            EnvironmentSampler,
            ErrorMessage);
        bDrawn = bDrawn && Draw(
            Commands,
            *Irradiance.Texture,
            *IrradiancePipeline,
            Irradiance.Width,
            Irradiance.Height,
            0.0F,
            EnvironmentDescriptor,
            EnvironmentSampler,
            ErrorMessage);
        for (uint32 Level = 0; bDrawn && Level < 4; ++Level)
        {
            bDrawn = Draw(
                Commands,
                *Prefiltered[Level].Texture,
                *PrefilterPipeline,
                Prefiltered[Level].Width,
                Prefiltered[Level].Height,
                Roughness[Level],
                EnvironmentDescriptor,
                EnvironmentSampler,
                ErrorMessage);
        }
        Commands.End();
        if (!bDrawn)
        {
            return false;
        }

        Device.SubmitCommandList(Commands);
        Device.WaitIdle();
        Binding.Irradiance = Irradiance.Descriptor;
        Binding.Prefiltered0 = Prefiltered[0].Descriptor;
        Binding.Prefiltered1 = Prefiltered[1].Descriptor;
        Binding.Prefiltered2 = Prefiltered[2].Descriptor;
        Binding.Prefiltered3 = Prefiltered[3].Descriptor;
        Binding.BrdfLut = BrdfLut.Descriptor;
        Binding.EnvironmentSampler = EnvironmentSampler;
        Binding.BrdfSampler = ClampSampler->GetDescriptorIndex();
        bReady = true;
        return true;
    }

    bool ImageBasedLighting::CreateTarget(
        RHIDevice& Device,
        Target& Out,
        uint32 Width,
        uint32 Height,
        std::string* ErrorMessage)
    {
        Out.Texture = Device.CreateColorTarget(Width, Height, ERHIFormat::R16G16B16A16_Float, ErrorMessage);
        if (Out.Texture == nullptr || !Device.CreateShaderResourceView(*Out.Texture, Out.Descriptor, ErrorMessage))
        {
            return false;
        }

        Out.Width = Width;
        Out.Height = Height;
        return true;
    }

    bool ImageBasedLighting::CreateFullscreenPipeline(
        RHIDevice& Device,
        ShaderManager& Shaders,
        const char* PixelPath,
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
        PipelineDesc.RenderTargetFormat = ERHIFormat::R16G16B16A16_Float;
        PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
        PipelineDesc.bEnableDepth = false;
        Out = Device.CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
        return Out != nullptr;
    }

    bool ImageBasedLighting::Draw(
        RHICommandList& Commands,
        RHITexture& TargetTexture,
        RHIPipeline& Pipeline,
        uint32 TargetWidth,
        uint32 TargetHeight,
        float Roughness,
        uint32 Source,
        uint32 Sampler,
        std::string* ErrorMessage)
    {
        IblGpuConstants Constants{};
        Constants.DestSize[0] = static_cast<float>(TargetWidth);
        Constants.DestSize[1] = static_cast<float>(TargetHeight);
        Constants.Roughness = Roughness;
        Constants.Source = Source;
        Constants.Sampler = Sampler;
        const std::span<const uint8> Bytes(reinterpret_cast<const uint8*>(&Constants), sizeof(Constants));
        if (ConstantBuffer == nullptr || ConstantSlot >= 8 || !ConstantBuffer->Update(ConstantSlot * 256, Bytes))
        {
            SetErrorMessage(ErrorMessage, "Failed to write image based lighting constants");
            return false;
        }

        const uint32 Offset = ConstantSlot * 256;
        ++ConstantSlot;
        Commands.BeginRenderPass(TargetTexture, nullptr, {}, true, false);
        Commands.SetViewport({0.0F, 0.0F, static_cast<float>(TargetWidth), static_cast<float>(TargetHeight), 0.0F, 1.0F});
        Commands.SetScissor({0, 0, static_cast<int32>(TargetWidth), static_cast<int32>(TargetHeight)});
        Commands.SetPipeline(Pipeline);
        Commands.BindShaderResources();
        Commands.SetConstantBuffer(*ConstantBuffer, Offset);
        Commands.Draw(3);
        Commands.EndRenderPass();
        Commands.UseAsShaderResource(TargetTexture);
        return true;
    }
}
