#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/RHI/Public/RHIShader.h"
#include "Framework/Shader/Public/IShaderCompiler.h"
#include "Framework/Shader/Public/ShaderHandle.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

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

            [[nodiscard]] ShaderHandle Register(
                const std::filesystem::path& VertexPath,
                const std::filesystem::path& PixelPath,
                const std::filesystem::path& MaterialPath);

            bool LoadShaders(
                RHIDevice& Device,
                ShaderHandle Handle,
                std::string* ErrorMessage = nullptr);

            [[nodiscard]] RHIShader* GetVertexShader(ShaderHandle Handle) const;
            [[nodiscard]] RHIShader* GetPixelShader(ShaderHandle Handle) const;

        private:
            std::unique_ptr<RHIShader> LoadShader(
                RHIDevice& Device,
                const std::filesystem::path& RelativePath,
                const std::filesystem::path& ImplementationPath,
                std::string_view EntryPoint,
                ERHIShaderStage Stage,
                std::string* ErrorMessage);

            struct ShaderRecord
            {
                std::filesystem::path VertexPath;
                std::filesystem::path PixelPath;
                std::filesystem::path MaterialPath;
                std::unique_ptr<RHIShader> VertexShader;
                std::unique_ptr<RHIShader> PixelShader;
            };

            std::unique_ptr<IShaderCompiler> Compiler;
            std::filesystem::path ShaderDirectory;
            std::vector<ShaderRecord> Records;
    };
}
