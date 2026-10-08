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
        Records.clear();
        if (Compiler != nullptr)
        {
            Compiler->Finalize();
            Compiler.reset();
        }
    }

    ShaderHandle ShaderManager::Register(
        const std::filesystem::path& VertexPath,
        const std::filesystem::path& PixelPath,
        const std::filesystem::path& MaterialPath)
    {
        const std::string VertexKey = VertexPath.generic_string();
        const std::string PixelKey = PixelPath.generic_string();
        const std::string MaterialKey = MaterialPath.generic_string();
        for (uint32 Index = 0; Index < Records.size(); ++Index)
        {
            if (Records[Index].VertexPath.generic_string() == VertexKey &&
                Records[Index].PixelPath.generic_string() == PixelKey &&
                Records[Index].MaterialPath.generic_string() == MaterialKey)
            {
                ShaderHandle Handle;
                Handle.Index = Index;
                return Handle;
            }
        }

        ShaderRecord Record;
        Record.VertexPath = VertexPath;
        Record.PixelPath = PixelPath;
        Record.MaterialPath = MaterialPath;
        Records.push_back(std::move(Record));
        ShaderHandle Handle;
        Handle.Index = static_cast<uint32>(Records.size() - 1);
        return Handle;
    }

    bool ShaderManager::LoadShaders(RHIDevice& Device, ShaderHandle Handle, std::string* ErrorMessage)
    {
        if (!Handle.IsValid() || Handle.Index >= Records.size())
        {
            SetErrorMessage(ErrorMessage, "Shader handle is invalid");
            return false;
        }

        ShaderRecord& Record = Records[Handle.Index];
        if (Record.VertexShader != nullptr && Record.PixelShader != nullptr)
        {
            return true;
        }

        Record.VertexShader = LoadShader(
            Device,
            Record.VertexPath,
            {},
            "VertexMain",
            ERHIShaderStage::Vertex,
            ErrorMessage);
        if (Record.VertexShader == nullptr)
        {
            return false;
        }

        Record.PixelShader = LoadShader(
            Device,
            Record.PixelPath,
            Record.MaterialPath,
            "PixelMain",
            ERHIShaderStage::Pixel,
            ErrorMessage);
        return Record.PixelShader != nullptr;
    }

    RHIShader* ShaderManager::GetVertexShader(ShaderHandle Handle) const
    {
        if (!Handle.IsValid() || Handle.Index >= Records.size())
        {
            return nullptr;
        }

        return Records[Handle.Index].VertexShader.get();
    }

    RHIShader* ShaderManager::GetPixelShader(ShaderHandle Handle) const
    {
        if (!Handle.IsValid() || Handle.Index >= Records.size())
        {
            return nullptr;
        }

        return Records[Handle.Index].PixelShader.get();
    }

    std::unique_ptr<RHIShader> ShaderManager::LoadShader(
        RHIDevice& Device,
        const std::filesystem::path& RelativePath,
        const std::filesystem::path& ImplementationPath,
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
        if (!ImplementationPath.empty())
        {
            Request.ImplementationPath = ShaderDirectory / ImplementationPath;
        }
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
