#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "Framework/RHI/Public/RHIShader.h"
#include "Framework/RHI/Public/RHITypes.h"

#include <memory>
#include <string>

namespace ShadowEngine
{
    class RHIDevice;
    class RHISwapChain;
    class ShaderManager;

    class Renderer final : public NonCopyable
    {
        public:
            Renderer() = default;
            ~Renderer();

            bool Initialize(
                RHIDevice& InDevice,
                RHISwapChain& InSwapChain,
                ShaderManager& Shaders,
                const RHIColor& InClearColor,
                std::string* ErrorMessage = nullptr);
            void Finalize();

            bool RenderFrame();

        private:
            RHIDevice* Device = nullptr;
            RHISwapChain* SwapChain = nullptr;
            RHIColor ClearColor;

            std::unique_ptr<RHICommandList> CommandList;
            std::unique_ptr<RHIShader> VertexShader;
            std::unique_ptr<RHIShader> PixelShader;
            std::unique_ptr<RHIPipeline> Pipeline;
    };
}
