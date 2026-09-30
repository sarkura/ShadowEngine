#include "Framework/Shader/Public/ShaderManager.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/RHI/Public/RHIDevice.h"

#include <utility>

namespace ShadowEngine
{
    ShaderManager::~ShaderManager()
    {
        Finalize();
    }

    bool ShaderManager::Initialize(
        std::unique_ptr<IShaderCompiler> InCompiler,
        std::filesystem::path InShaderDirectory,
        std::string* ErrorMessage)
    {
        if (InCompiler == nullptr)
        {
            SetErrorMessage(ErrorMessage, "No shader compiler is registered");
            return false;
        }

        if (!std::filesystem::is_directory(InShaderDirectory))
        {
            SetErrorMessage(
                ErrorMessage,
                "Shader directory does not exist: " + InShaderDirectory.string());
            return false;
        }

        if (!InCompiler->Initialize(ErrorMessage))
        {
            return false;
        }

        Compiler = std::move(InCompiler);
        ShaderDirectory = std::move(InShaderDirectory);
        return true;
    }

    void ShaderManager::Finalize()
    {
        if (Compiler != nullptr)
        {
            Compiler->Finalize();
            Compiler.reset();
        }
    }

    std::unique_ptr<RHIShader> ShaderManager::LoadShader(
        RHIDevice& Device,
        const std::filesystem::path& RelativePath,
        std::string_view EntryPoint,
        ERHIShaderStage Stage,
        std::string* ErrorMessage)
    {
        if (Compiler == nullptr)
        {
            SetErrorMessage(ErrorMessage, "ShaderManager is not initialized");
            return nullptr;
        }

        ShaderCompileRequest Request;
        Request.SourcePath = ShaderDirectory / RelativePath;
        Request.EntryPoint = EntryPoint;
        Request.Stage = Stage;
        Request.Format = Device.GetShaderFormat();

        ShaderBytecode Bytecode;
        if (!Compiler->Compile(Request, Bytecode, ErrorMessage))
        {
            return nullptr;
        }

        Log::Info(
            "Compiled shader {}:{} ({} bytes)",
            RelativePath.generic_string(),
            EntryPoint,
            Bytecode.Data.size());

        RHIShaderDesc Desc;
        Desc.Stage = Bytecode.Stage;
        Desc.EntryPoint = Bytecode.EntryPoint;
        Desc.Bytecode = Bytecode.Data;
        return Device.CreateShader(Desc, ErrorMessage);
    }
}
