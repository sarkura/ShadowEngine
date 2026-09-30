#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/RHI/Public/RHIShader.h"
#include "Framework/Shader/Public/IShaderCompiler.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace ShadowEngine
{
    class RHIDevice;

    class ShaderManager final : public NonCopyable
    {
        public:
            ShaderManager() = default;
            ~ShaderManager();

            bool Initialize(
                std::unique_ptr<IShaderCompiler> InCompiler,
                std::filesystem::path InShaderDirectory,
                std::string* ErrorMessage = nullptr);
            void Finalize();

            std::unique_ptr<RHIShader> LoadShader(
                RHIDevice& Device,
                const std::filesystem::path& RelativePath,
                std::string_view EntryPoint,
                ERHIShaderStage Stage,
                std::string* ErrorMessage = nullptr);

        private:
            std::unique_ptr<IShaderCompiler> Compiler;
            std::filesystem::path ShaderDirectory;
    };
}
