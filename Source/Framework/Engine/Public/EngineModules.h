#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/Shader/Public/IShaderCompiler.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace ShadowEngine
{
    using RHIDeviceFactory = std::function<std::unique_ptr<RHIDevice>()>;
    using ShaderCompilerFactory = std::function<std::unique_ptr<IShaderCompiler>()>;

    class EngineModules final : public NonCopyable
    {
        public:
            static EngineModules& Get();

            void RegisterRHI(std::string Name, RHIDeviceFactory Factory);
            void RegisterShaderCompiler(ShaderCompilerFactory Factory);

            [[nodiscard]] std::unique_ptr<RHIDevice> CreateRHIDevice(std::string_view Name) const;
            [[nodiscard]] std::unique_ptr<IShaderCompiler> CreateShaderCompiler() const;

        private:
            EngineModules() = default;

            std::map<std::string, RHIDeviceFactory, std::less<>> RHIFactories;
            ShaderCompilerFactory CompilerFactory;
    };
}
