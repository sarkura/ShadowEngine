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

        if (Parameters.MemberCount() != 0)
        {
            const auto Member = Parameters.MemberBegin();
            SetErrorMessage(
                ErrorMessage,
                "Material parameter '" + std::string(Member->name.GetString()) +
                    "' is unsupported: " + FilePath.generic_string());
            return false;
        }

        Description.ShaderPath = Shader.GetString();
        Description.Parameter = MaterialParameter{};
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
