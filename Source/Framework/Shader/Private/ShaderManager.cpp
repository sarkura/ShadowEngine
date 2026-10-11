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
        const std::filesystem::path& MaterialPath,
        const std::filesystem::path& LightPath,
        const std::vector<std::string>& DescriptorFields,
        const std::vector<ShaderScalarField>& ScalarFields)
    {
        const std::string VertexKey = VertexPath.generic_string();
        const std::string PixelKey = PixelPath.generic_string();
        const std::string MaterialKey = MaterialPath.generic_string();
        const std::string LightKey = LightPath.generic_string();
        for (uint32 Index = 0; Index < Records.size(); ++Index)
        {
            if (Records[Index].VertexPath.generic_string() == VertexKey &&
                Records[Index].PixelPath.generic_string() == PixelKey &&
                Records[Index].MaterialPath.generic_string() == MaterialKey &&
                Records[Index].LightPath.generic_string() == LightKey &&
                Records[Index].DescriptorFields == DescriptorFields &&
                Records[Index].ScalarFields == ScalarFields)
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
        Record.LightPath = LightPath;
        Record.DescriptorFields = DescriptorFields;
        Record.ScalarFields = ScalarFields;
        Records.push_back(std::move(Record));
        ShaderHandle Handle;
        Handle.Index = static_cast<uint32>(Records.size() - 1);
        return Handle;
    }

    void ShaderManager::SetShaderFormat(ERHIShaderFormat Format)
    {
        ShaderFormat = Format;
        bFormatSet = true;
    }

    bool ShaderManager::Compile(ShaderHandle Handle, std::string* ErrorMessage)
    {
        if (!Handle.IsValid() || Handle.Index >= Records.size())
        {
            SetErrorMessage(ErrorMessage, "Shader handle is invalid");
            return false;
        }

        return CompileRecord(Records[Handle.Index], ErrorMessage);
    }

    bool ShaderManager::LoadShaders(RHIDevice& Device, ShaderHandle Handle, std::string* ErrorMessage)
    {
        if (!bFormatSet)
        {
            SetShaderFormat(Device.GetShaderFormat());
        }

        if (!Compile(Handle, ErrorMessage))
        {
            return false;
        }

        ShaderRecord& Record = Records[Handle.Index];
        if (Record.VertexShader == nullptr)
        {
            Record.VertexShader = CreateShader(Device, Record.VertexBytecode, ErrorMessage);
            if (Record.VertexShader == nullptr)
            {
                return false;
            }
        }

        if (Record.PixelShader == nullptr)
        {
            Record.PixelShader = CreateShader(Device, Record.PixelBytecode, ErrorMessage);
        }

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

    const ShaderReflection* ShaderManager::GetReflection(ShaderHandle Handle, ERHIShaderStage Stage) const
    {
        if (!Handle.IsValid() || Handle.Index >= Records.size() || !Records[Handle.Index].bCompiled)
        {
            return nullptr;
        }

        const ShaderRecord& Record = Records[Handle.Index];
        return Stage == ERHIShaderStage::Vertex ? &Record.VertexReflection : &Record.PixelReflection;
    }

    bool ShaderManager::HasSource(const std::filesystem::path& RelativePath) const
    {
        return !RelativePath.empty() && std::filesystem::is_regular_file(ShaderDirectory / RelativePath);
    }

    bool ShaderManager::CompileRecord(ShaderRecord& Record, std::string* ErrorMessage)
    {
        if (Record.bCompiled)
        {
            return true;
        }

        if (Compiler == nullptr)
        {
            SetErrorMessage(ErrorMessage, "ShaderManager is not initialized");
            return false;
        }

        if (!bFormatSet)
        {
            SetErrorMessage(ErrorMessage, "Shader format is not set");
            return false;
        }

        ShaderCompileRequest VertexRequest;
        VertexRequest.SourcePath = ShaderDirectory / Record.VertexPath;
        VertexRequest.EntryPoint = "VertexMain";
        VertexRequest.Stage = ERHIShaderStage::Vertex;
        VertexRequest.Format = ShaderFormat;
        if (!Compiler->Compile(VertexRequest, Record.VertexBytecode, Record.VertexReflection, ErrorMessage))
        {
            return false;
        }

        ShaderCompileRequest PixelRequest;
        PixelRequest.SourcePath = ShaderDirectory / Record.PixelPath;
        PixelRequest.ImplementationPath = Record.MaterialPath.empty() ? std::filesystem::path() : ShaderDirectory / Record.MaterialPath;
        PixelRequest.LightPath = Record.LightPath.empty() ? std::filesystem::path() : ShaderDirectory / Record.LightPath;
        PixelRequest.EntryPoint = "PixelMain";
        PixelRequest.DescriptorFields = Record.DescriptorFields;
        PixelRequest.ScalarFields = Record.ScalarFields;
        PixelRequest.Stage = ERHIShaderStage::Pixel;
        PixelRequest.Format = ShaderFormat;
        if (!Compiler->Compile(PixelRequest, Record.PixelBytecode, Record.PixelReflection, ErrorMessage))
        {
            return false;
        }

        Record.bCompiled = true;
        Log::Info(
            "Reflected shader {} ({} vertex bindings, {} pixel bindings)",
            Record.PixelPath.generic_string(),
            Record.VertexReflection.Bindings.size(),
            Record.PixelReflection.Bindings.size());
        return true;
    }

    std::unique_ptr<RHIShader> ShaderManager::CreateShader(
        RHIDevice& Device,
        const ShaderBytecode& Bytecode,
        std::string* ErrorMessage)
    {
        RHIShaderDesc Desc;
        Desc.Stage = Bytecode.Stage;
        Desc.EntryPoint = Bytecode.EntryPoint;
        Desc.Bytecode = Bytecode.Data;
        return Device.CreateShader(Desc, ErrorMessage);
    }
}
