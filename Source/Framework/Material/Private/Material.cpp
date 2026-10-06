#include "Framework/Material/Public/Material.h"

#include "Framework/Common/Public/Log.h"

#include <fstream>
#include <iterator>
#include <sstream>
#include <utility>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

namespace ShadowEngine
{
    namespace
    {
        bool ReadJsonObject(
            const std::filesystem::path& FilePath,
            rapidjson::Document& Document,
            std::string* ErrorMessage)
        {
            std::ifstream File(FilePath, std::ios::binary);
            if (!File)
            {
                SetErrorMessage(ErrorMessage, "Unable to open material description: " + FilePath.generic_string());
                return false;
            }

            const std::string Json{
                std::istreambuf_iterator<char>(File),
                std::istreambuf_iterator<char>()};

            Document.Parse(Json.data(), Json.size());
            if (Document.HasParseError())
            {
                std::ostringstream Message;
                Message << "Invalid JSON in " << FilePath.generic_string()
                        << " at offset " << Document.GetErrorOffset() << ": "
                        << rapidjson::GetParseError_En(Document.GetParseError());
                SetErrorMessage(ErrorMessage, Message.str());
                return false;
            }

            if (!Document.IsObject())
            {
                SetErrorMessage(ErrorMessage, "Material description must be an object: " + FilePath.generic_string());
                return false;
            }

            return true;
        }

        bool RequireMembers(
            const rapidjson::Value& Object,
            const std::filesystem::path& FilePath,
            const char* const* Names,
            size_t NameCount,
            std::string* ErrorMessage)
        {
            for (auto Member = Object.MemberBegin(); Member != Object.MemberEnd(); ++Member)
            {
                bool bKnown = false;
                for (size_t Index = 0; Index < NameCount; ++Index)
                {
                    if (Member->name == Names[Index])
                    {
                        bKnown = true;
                        break;
                    }
                }
                if (!bKnown)
                {
                    SetErrorMessage(
                        ErrorMessage,
                        "Material description has unsupported member '" + std::string(Member->name.GetString()) +
                            "': " + FilePath.generic_string());
                    return false;
                }
            }

            for (size_t Index = 0; Index < NameCount; ++Index)
            {
                if (!Object.HasMember(Names[Index]))
                {
                    SetErrorMessage(
                        ErrorMessage,
                        "Material description is missing '" + std::string(Names[Index]) +
                            "': " + FilePath.generic_string());
                    return false;
                }
            }

            return true;
        }

        bool ReadVec3(
            const rapidjson::Value& Object,
            const char* Name,
            float Out[3],
            const std::filesystem::path& FilePath,
            std::string* ErrorMessage)
        {
            const auto Found = Object.FindMember(Name);
            if (Found == Object.MemberEnd() || !Found->value.IsArray() || Found->value.Size() != 3)
            {
                SetErrorMessage(
                    ErrorMessage,
                    std::string(Name) + " must contain 3 numbers: " + FilePath.generic_string());
                return false;
            }

            for (rapidjson::SizeType Index = 0; Index < 3; ++Index)
            {
                if (!Found->value[Index].IsNumber())
                {
                    SetErrorMessage(
                        ErrorMessage,
                        std::string(Name) + " must contain 3 numbers: " + FilePath.generic_string());
                    return false;
                }
                Out[Index] = Found->value[Index].GetFloat();
            }

            return true;
        }

        bool ReadUnitFloat(
            const rapidjson::Value& Object,
            const char* Name,
            float& Out,
            const std::filesystem::path& FilePath,
            std::string* ErrorMessage)
        {
            const auto Found = Object.FindMember(Name);
            if (Found == Object.MemberEnd() || !Found->value.IsNumber())
            {
                SetErrorMessage(
                    ErrorMessage,
                    std::string(Name) + " must be a number: " + FilePath.generic_string());
                return false;
            }

            Out = Found->value.GetFloat();
            if (Out < 0.0F || Out > 1.0F)
            {
                SetErrorMessage(
                    ErrorMessage,
                    std::string(Name) + " must be between 0 and 1: " + FilePath.generic_string());
                return false;
            }

            return true;
        }
    }

    bool LoadMaterialDescription(
        const std::filesystem::path& FilePath,
        MaterialDescription& Description,
        std::string* ErrorMessage)
    {
        rapidjson::Document Document;
        if (!ReadJsonObject(FilePath, Document, ErrorMessage))
        {
            return false;
        }

        const char* const Members[] = {"type", "shader", "parameters"};
        if (!RequireMembers(Document, FilePath, Members, 3, ErrorMessage))
        {
            return false;
        }

        const rapidjson::Value& Type = Document["type"];
        if (!Type.IsString() || std::string(Type.GetString()) != "Material")
        {
            SetErrorMessage(ErrorMessage, "Material description type must be Material: " + FilePath.generic_string());
            return false;
        }

        const rapidjson::Value& Shader = Document["shader"];
        if (!Shader.IsString() || Shader.GetStringLength() == 0)
        {
            SetErrorMessage(ErrorMessage, "Material description shader must be a path: " + FilePath.generic_string());
            return false;
        }

        const rapidjson::Value& Parameters = Document["parameters"];
        if (!Parameters.IsObject())
        {
            SetErrorMessage(ErrorMessage, "Material parameters must be an object: " + FilePath.generic_string());
            return false;
        }

        const char* const ParameterMembers[] = {"BaseColor", "Roughness", "SpecularColor"};
        for (auto Member = Parameters.MemberBegin(); Member != Parameters.MemberEnd(); ++Member)
        {
            bool bKnown = false;
            for (const char* Name : ParameterMembers)
            {
                if (Member->name == Name)
                {
                    bKnown = true;
                    break;
                }
            }
            if (!bKnown)
            {
                SetErrorMessage(
                    ErrorMessage,
                    "Material parameter '" + std::string(Member->name.GetString()) +
                        "' is unsupported: " + FilePath.generic_string());
                return false;
            }
        }

        MaterialParameter Parsed;
        if (!ReadVec3(Parameters, "BaseColor", Parsed.BaseColor, FilePath, ErrorMessage) ||
            !ReadUnitFloat(Parameters, "Roughness", Parsed.Roughness, FilePath, ErrorMessage) ||
            !ReadVec3(Parameters, "SpecularColor", Parsed.SpecularColor, FilePath, ErrorMessage))
        {
            return false;
        }

        Description.ShaderPath = Shader.GetString();
        Description.Parameter = Parsed;
        return true;
    }

    Material::Material(std::filesystem::path InShaderPath)
        : ShaderPath(std::move(InShaderPath))
    {
    }

    const std::filesystem::path& Material::GetShaderPath() const
    {
        return ShaderPath;
    }
}
