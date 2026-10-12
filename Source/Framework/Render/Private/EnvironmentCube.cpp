#include "Framework/Render/Public/EnvironmentCube.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIDefinitions.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "Framework/RHI/Public/RHIPipelineLayout.h"
#include "Framework/RHI/Public/RHITexture.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <cstring>
#include <span>

namespace ShadowEngine
{
    bool EnvironmentCube::Ensure(
        RHIDevice& Device,
        ShaderManager& Shaders,
        RHICommandList& Commands,
        uint32 EquirectDescriptor,
        uint32 EquirectSampler,
        uint32 FaceSize,
        std::string* ErrorMessage)
    {
        static_assert(sizeof(GpuConstants) == 32);
        if (bReady)
        {
            return true;
        }

        if (FaceSize == 0)
        {
            SetErrorMessage(ErrorMessage, "Environment cube face size is zero");
            return false;
        }

        if (Pipeline == nullptr)
        {
            const ShaderHandle Shader = Shaders.Register("PostProcess/Fullscreen.slang", "Sky/EquirectToCube.slang", {}, {});
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
            PipelineDesc.RenderTargetFormat = ERHIFormat::R32G32B32A32_Float;
            PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
            PipelineDesc.bEnableDepth = false;
            Pipeline = Device.CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
            ConstantBuffer = Device.CreateConstantBuffer(256 * 6, ErrorMessage);
            if (Pipeline == nullptr || ConstantBuffer == nullptr)
            {
                return false;
            }
        }

        Texture = Device.CreateCubeTarget(FaceSize, ErrorMessage);
        if (Texture == nullptr || !Device.CreateShaderResourceView(*Texture, Descriptor, ErrorMessage))
        {
            return false;
        }

        Commands.Begin();
        for (uint32 Face = 0; Face < 6; ++Face)
        {
            GpuConstants Constants{};
            Constants.DestSize[0] = static_cast<float>(FaceSize);
            Constants.DestSize[1] = static_cast<float>(FaceSize);
            Constants.Face = Face;
            Constants.Source = EquirectDescriptor;
            Constants.Sampler = EquirectSampler;
            const std::span<const uint8> Bytes(reinterpret_cast<const uint8*>(&Constants), sizeof(Constants));
            if (!ConstantBuffer->Update(Face * 256, Bytes))
            {
                Commands.End();
                SetErrorMessage(ErrorMessage, "Failed to write environment cube constants");
                return false;
            }

            Commands.BeginRenderPass(*Texture, nullptr, {}, false, false, Face);
            Commands.SetViewport({0.0F, 0.0F, static_cast<float>(FaceSize), static_cast<float>(FaceSize), 0.0F, 1.0F});
            Commands.SetScissor({0, 0, static_cast<int32>(FaceSize), static_cast<int32>(FaceSize)});
            Commands.SetPipeline(*Pipeline);
            Commands.BindShaderResources();
            Commands.SetConstantBuffer(*ConstantBuffer, Face * 256);
            Commands.Draw(3);
            Commands.EndRenderPass();
        }
        Commands.UseAsShaderResource(*Texture);
        Commands.End();
        Device.SubmitCommandList(Commands);
        Device.WaitIdle();
        bReady = true;
        return true;
    }
}
