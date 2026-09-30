#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/RHI/Public/RHIAdapter.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIDefinitions.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "Framework/RHI/Public/RHIShader.h"
#include "Framework/RHI/Public/RHISwapChain.h"
#include "Framework/RHI/Public/RHITexture.h"

#include <memory>
#include <string>
#include <string_view>

namespace ShadowEngine
{
    class RHIDevice : public NonCopyable
    {
        public:
            virtual ~RHIDevice() = default;

            virtual bool Initialize(const RHIDeviceDesc& Desc, std::string* ErrorMessage = nullptr) = 0;
            virtual void Finalize() = 0;

            [[nodiscard]] virtual std::string_view GetName() const = 0;
            [[nodiscard]] virtual ERHIShaderFormat GetShaderFormat() const = 0;
            [[nodiscard]] virtual const RHIAdapter* GetAdapter() const = 0;

            virtual std::unique_ptr<RHISwapChain> CreateSwapChain(
                const RHISwapChainDesc& Desc,
                std::string* ErrorMessage = nullptr) = 0;

            virtual std::unique_ptr<RHIShader> CreateShader(
                const RHIShaderDesc& Desc,
                std::string* ErrorMessage = nullptr) = 0;

            virtual std::unique_ptr<RHIPipeline> CreateGraphicsPipeline(
                const RHIGraphicsPipelineDesc& Desc,
                std::string* ErrorMessage = nullptr) = 0;

            virtual std::unique_ptr<RHICommandList> CreateCommandList(
                std::string* ErrorMessage = nullptr) = 0;

            virtual void SubmitCommandList(RHICommandList& CommandList) = 0;
            virtual void WaitIdle() = 0;
    };
}
