#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/RHI/Public/RHIShader.h"
#include "Framework/Shader/Public/IShaderCompiler.h"
#include "Framework/Shader/Public/Shader.h"
#include "Framework/Shader/Public/ShaderHandle.h"
#include "Framework/Shader/Public/ShaderReflection.h"

#include <filesystem>
#include <memory>
#include <string>
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

            void SetShaderFormat(ERHIShaderFormat Format);

            [[nodiscard]] ShaderHandle Register(
                const std::filesystem::path& VertexPath,
                const std::filesystem::path& PixelPath,
                const std::filesystem::path& MaterialPath,
                const std::filesystem::path& LightPath,
                const std::vector<std::string>& DescriptorFields = {},
                const std::vector<ShaderScalarField>& ScalarFields = {});

            bool Compile(ShaderHandle Handle, std::string* ErrorMessage = nullptr);

            bool LoadShaders(
                RHIDevice& Device,
                ShaderHandle Handle,
                std::string* ErrorMessage = nullptr);

            [[nodiscard]] RHIShader* GetVertexShader(ShaderHandle Handle) const;
            [[nodiscard]] RHIShader* GetPixelShader(ShaderHandle Handle) const;
            [[nodiscard]] const ShaderReflection* GetReflection(ShaderHandle Handle, ERHIShaderStage Stage) const;
            [[nodiscard]] bool HasSource(const std::filesystem::path& RelativePath) const;

        private:
            struct ShaderRecord
            {
                std::filesystem::path VertexPath;
                std::filesystem::path PixelPath;
                std::filesystem::path MaterialPath;
                std::filesystem::path LightPath;
                std::vector<std::string> DescriptorFields;
                std::vector<ShaderScalarField> ScalarFields;
                ShaderBytecode VertexBytecode;
                ShaderBytecode PixelBytecode;
                ShaderReflection VertexReflection;
                ShaderReflection PixelReflection;
                std::unique_ptr<RHIShader> VertexShader;
                std::unique_ptr<RHIShader> PixelShader;
                bool bCompiled = false;
            };

            bool CompileRecord(ShaderRecord& Record, std::string* ErrorMessage);

            std::unique_ptr<RHIShader> CreateShader(
                RHIDevice& Device,
                const ShaderBytecode& Bytecode,
                std::string* ErrorMessage);

            std::unique_ptr<IShaderCompiler> Compiler;
            std::filesystem::path ShaderDirectory;
            ERHIShaderFormat ShaderFormat = ERHIShaderFormat::DXIL;
            bool bFormatSet = false;
            std::vector<ShaderRecord> Records;
    };
}
