#include "Framework/Config/Public/DescriptionParser.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/Config/Private/JsonDocument.h"

#include <utility>

namespace ShadowEngine
{
    bool DescriptionParser::LoadMeshDescription(
        const std::filesystem::path& FilePath,
        MeshDescription& Description,
        std::string* ErrorMessage)
    {
        rapidjson::Document Document;
        if (!ParseJsonObject(FilePath, "Mesh description", Document, ErrorMessage))
        {
            return false;
        }

        const char* const Members[] = {"type", "source", "materials"};
        if (!RequireMembers(Document, FilePath, "Mesh description", Members, 3, ErrorMessage))
        {
            return false;
        }

        const rapidjson::Value& Type = Document["type"];
        if (!Type.IsString() || std::string(Type.GetString()) != "Mesh")
        {
            SetErrorMessage(ErrorMessage, "Mesh description type must be Mesh: " + FilePath.generic_string());
            return false;
        }

        const rapidjson::Value& Source = Document["source"];
        if (!Source.IsString() || Source.GetStringLength() == 0)
        {
            SetErrorMessage(ErrorMessage, "Mesh description source must be a path: " + FilePath.generic_string());
            return false;
        }

        const rapidjson::Value& Materials = Document["materials"];
        if (!Materials.IsArray() || Materials.Empty())
        {
            SetErrorMessage(
                ErrorMessage,
                "Mesh description materials must be a non-empty array: " + FilePath.generic_string());
            return false;
        }

        const char* const SlotMembers[] = {"slot", "material"};
        MeshDescription Parsed;
        Parsed.Materials.reserve(Materials.Size());
        for (rapidjson::SizeType Index = 0; Index < Materials.Size(); ++Index)
        {
            const rapidjson::Value& Entry = Materials[Index];
            if (!Entry.IsObject())
            {
                SetErrorMessage(ErrorMessage, "Mesh material entry must be an object: " + FilePath.generic_string());
                return false;
            }

            if (!RequireMembers(Entry, FilePath, "Mesh material entry", SlotMembers, 2, ErrorMessage))
            {
                return false;
            }

            const rapidjson::Value& Slot = Entry["slot"];
            const rapidjson::Value& Material = Entry["material"];
            if (!Slot.IsUint())
            {
                SetErrorMessage(
                    ErrorMessage,
                    "Mesh material slot must be an unsigned integer: " + FilePath.generic_string());
                return false;
            }
            if (!Material.IsString() || Material.GetStringLength() == 0)
            {
                SetErrorMessage(ErrorMessage, "Mesh material path is empty: " + FilePath.generic_string());
                return false;
            }

            for (const MeshMaterialBinding& Existing : Parsed.Materials)
            {
                if (Existing.Slot == Slot.GetUint())
                {
                    SetErrorMessage(
                        ErrorMessage,
                        "Mesh material slot " + std::to_string(Slot.GetUint()) +
                            " is duplicated: " + FilePath.generic_string());
                    return false;
                }
            }

            MeshMaterialBinding Binding;
            Binding.Slot = Slot.GetUint();
            Binding.Material = Material.GetString();
            Parsed.Materials.push_back(std::move(Binding));
        }

        Parsed.Source = Source.GetString();
        Description = std::move(Parsed);
        return true;
    }

    bool DescriptionParser::LoadMaterialDescription(
        const std::filesystem::path& FilePath,
        MaterialDescription& Description,
        std::string* ErrorMessage)
    {
        rapidjson::Document Document;
        if (!ParseJsonObject(FilePath, "Material description", Document, ErrorMessage))
        {
            return false;
        }

        const char* const Members[] = {"type", "shader", "parameters"};
        if (!RequireMembers(Document, FilePath, "Material description", Members, 3, ErrorMessage))
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

        const char* const RequiredParameters[] = {"BaseColor", "Roughness", "SpecularColor"};
        const char* const ParameterMembers[] = {
            "BaseColor",
            "Roughness",
            "SpecularColor",
            "BaseColorTexture",
            "RoughnessTexture",
        };
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

        for (const char* Name : RequiredParameters)
        {
            if (!Parameters.HasMember(Name))
            {
                SetErrorMessage(
                    ErrorMessage,
                    std::string(Name) + " must be present: " + FilePath.generic_string());
                return false;
            }
        }

        const auto ReadVec3 = [&](const char* Name, std::vector<float>& Out) -> bool
        {
            const auto Found = Parameters.FindMember(Name);
            if (Found == Parameters.MemberEnd() || !Found->value.IsArray() || Found->value.Size() != 3)
            {
                SetErrorMessage(ErrorMessage, std::string(Name) + " must contain 3 numbers: " + FilePath.generic_string());
                return false;
            }

            Out.resize(3);
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
        };

        const auto ReadUnitFloat = [&](const char* Name, float& Out) -> bool
        {
            const auto Found = Parameters.FindMember(Name);
            if (Found == Parameters.MemberEnd() || !Found->value.IsNumber())
            {
                SetErrorMessage(ErrorMessage, std::string(Name) + " must be a number: " + FilePath.generic_string());
                return false;
            }

            Out = Found->value.GetFloat();
            if (Out < 0.0F || Out > 1.0F)
            {
                SetErrorMessage(ErrorMessage, std::string(Name) + " must be between 0 and 1: " + FilePath.generic_string());
                return false;
            }

            return true;
        };

        const auto ReadOptionalPath = [&](const char* Name, std::string& Out) -> bool
        {
            const auto Found = Parameters.FindMember(Name);
            if (Found == Parameters.MemberEnd())
            {
                Out.clear();
                return true;
            }

            if (!Found->value.IsString() || Found->value.GetStringLength() == 0)
            {
                SetErrorMessage(ErrorMessage, std::string(Name) + " must be a path: " + FilePath.generic_string());
                return false;
            }

            Out.assign(Found->value.GetString(), Found->value.GetStringLength());
            return true;
        };

        MaterialDescription Parsed;
        if (!ReadVec3("BaseColor", Parsed.BaseColor) ||
            !ReadUnitFloat("Roughness", Parsed.Roughness) ||
            !ReadVec3("SpecularColor", Parsed.SpecularColor) ||
            !ReadOptionalPath("BaseColorTexture", Parsed.BaseColorTexture) ||
            !ReadOptionalPath("RoughnessTexture", Parsed.RoughnessTexture))
        {
            return false;
        }

        if (Parsed.BaseColorTexture.empty() != Parsed.RoughnessTexture.empty())
        {
            SetErrorMessage(
                ErrorMessage,
                "BaseColorTexture and RoughnessTexture must be set together: " + FilePath.generic_string());
            return false;
        }

        Parsed.Shader = Shader.GetString();
        Description = std::move(Parsed);
        return true;
    }
}
