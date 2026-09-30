#include "Framework/Render/Public/Renderer.h"

#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/Shader/Public/ShaderManager.h"

namespace ShadowEngine
{
    namespace
    {
        constexpr char TriangleShaderPath[] = "Debug/Triangle.slang";
        constexpr char VertexEntryPoint[] = "VertexMain";
        constexpr char PixelEntryPoint[] = "PixelMain";
    }

    Renderer::~Renderer()
    {
        Finalize();
    }

    bool Renderer::Initialize(
        RHIDevice& InDevice,
        RHISwapChain& InSwapChain,
        ShaderManager& Shaders,
        const RHIColor& InClearColor,
        std::string* ErrorMessage)
    {
        Device = &InDevice;
        SwapChain = &InSwapChain;
        ClearColor = InClearColor;

        VertexShader = Shaders.LoadShader(
            InDevice,
            TriangleShaderPath,
            VertexEntryPoint,
            ERHIShaderStage::Vertex,
            ErrorMessage);
        if (VertexShader == nullptr)
        {
            return false;
        }

        PixelShader = Shaders.LoadShader(
            InDevice,
            TriangleShaderPath,
            PixelEntryPoint,
            ERHIShaderStage::Pixel,
            ErrorMessage);
        if (PixelShader == nullptr)
        {
            return false;
        }

        RHIGraphicsPipelineDesc PipelineDesc;
        PipelineDesc.VertexShader = VertexShader.get();
        PipelineDesc.PixelShader = PixelShader.get();
        PipelineDesc.RenderTargetFormat = InSwapChain.GetFormat();
        PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
        Pipeline = InDevice.CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
        if (Pipeline == nullptr)
        {
            return false;
        }

        CommandList = InDevice.CreateCommandList(ErrorMessage);
        return CommandList != nullptr;
    }

    void Renderer::Finalize()
    {
        CommandList.reset();
        Pipeline.reset();
        PixelShader.reset();
        VertexShader.reset();
        SwapChain = nullptr;
        Device = nullptr;
    }

    bool Renderer::RenderFrame()
    {
        if (Device == nullptr || SwapChain == nullptr || CommandList == nullptr)
        {
            return false;
        }

        const float Width = static_cast<float>(SwapChain->GetWidth());
        const float Height = static_cast<float>(SwapChain->GetHeight());

        CommandList->Begin();
        CommandList->BeginRenderPass(SwapChain->GetCurrentBackBuffer(), ClearColor);
        CommandList->SetViewport({0.0F, 0.0F, Width, Height, 0.0F, 1.0F});
        CommandList->SetScissor({
            0,
            0,
            static_cast<int32>(SwapChain->GetWidth()),
            static_cast<int32>(SwapChain->GetHeight())});
        CommandList->SetPipeline(*Pipeline);
        CommandList->Draw(3);
        CommandList->EndRenderPass();
        CommandList->End();

        Device->SubmitCommandList(*CommandList);
        const bool bPresented = SwapChain->Present();
        Device->WaitIdle();
        return bPresented;
    }
}
