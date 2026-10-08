#include "Framework/Config/Public/DescriptionParser.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/Config/Private/JsonDocument.h"

#include <string>
#include <utility>

namespace ShadowEngine
{
    namespace
    {
        bool ReadRendererBinding(
            const rapidjson::Value& Object,
            const std::filesystem::path& FilePath,
            const char* RendererName,
            MaterialRendererBinding& Binding,
            std::string* ErrorMessage)
        {
            const char* const Members[] = {"Vertex", "Fragment", "Material", "Parameters"};
            if (!RequireMembers(Object, FilePath, RendererName, Members, 4, ErrorMessage))
            {
                return false;
            }

            const rapidjson::Value& VertexShader = Object["Vertex"];
            if (!VertexShader.IsString() || VertexShader.GetStringLength() == 0)
            {
                SetErrorMessage(ErrorMessage, std::string(RendererName) + " Vertex must be a path: " + FilePath.generic_string());
                return false;
            }

            const rapidjson::Value& PixelShader = Object["Fragment"];
            if (!PixelShader.IsString() || PixelShader.GetStringLength() == 0)
            {
                SetErrorMessage(ErrorMessage, std::string(RendererName) + " Fragment must be a path: " + FilePath.generic_string());
                return false;
            }

            const rapidjson::Value& MaterialShader = Object["Material"];
            if (!MaterialShader.IsString() || MaterialShader.GetStringLength() == 0)
            {
                SetErrorMessage(ErrorMessage, std::string(RendererName) + " Material must be a path: " + FilePath.generic_string());
                return false;
            }

            const rapidjson::Value& Parameters = Object["Parameters"];
            if (!Parameters.IsObject())
            {
                SetErrorMessage(ErrorMessage, std::string(RendererName) + " Parameters must be an object: " + FilePath.generic_string());
                return false;
            }

            Binding.Renderer = RendererName;
            Binding.VertexShader = VertexShader.GetString();
            Binding.PixelShader = PixelShader.GetString();
            Binding.MaterialShader = MaterialShader.GetString();
            if (Parameters.MemberCount() == 0)
            {
                return true;
            }

            const char* const RequiredParameters[] = {"BaseColor", "Roughness", "SpecularColor"};
            const char* const ParameterMembers[] = {
                "BaseColor",
                "Roughness",
                "SpecularColor",
                "BaseColorTexture",
                "RoughnessTexture",
                "NormalTexture",
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

            if (!ReadVec3("BaseColor", Binding.BaseColor) ||
                !ReadUnitFloat("Roughness", Binding.Roughness) ||
                !ReadVec3("SpecularColor", Binding.SpecularColor) ||
                !ReadOptionalPath("BaseColorTexture", Binding.BaseColorTexture) ||
                !ReadOptionalPath("RoughnessTexture", Binding.RoughnessTexture) ||
                !ReadOptionalPath("NormalTexture", Binding.NormalTexture))
            {
                return false;
            }

            if (Binding.BaseColorTexture.empty() != Binding.RoughnessTexture.empty() ||
                Binding.BaseColorTexture.empty() != Binding.NormalTexture.empty())
            {
                SetErrorMessage(
                    ErrorMessage,
                    "BaseColorTexture, RoughnessTexture, and NormalTexture must be set together: " + FilePath.generic_string());
                return false;
            }

            Binding.bHasParameters = true;
            return true;
        }
    }

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

        if (!Document.HasMember("Type"))
        {
            SetErrorMessage(ErrorMessage, "Material description is missing 'Type': " + FilePath.generic_string());
            return false;
        }

        const rapidjson::Value& Type = Document["Type"];
        if (!Type.IsString() || std::string(Type.GetString()) != "Material")
        {
            SetErrorMessage(ErrorMessage, "Material description Type must be Material: " + FilePath.generic_string());
            return false;
        }

        const char* const RendererNames[] = {"BlinnPhongRenderer", "DebugRenderer"};
        MaterialDescription Parsed;
        for (auto Member = Document.MemberBegin(); Member != Document.MemberEnd(); ++Member)
        {
            if (Member->name == "Type")
            {
                continue;
            }

            bool bKnown = false;
            for (const char* Name : RendererNames)
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
                    "Material description has unsupported member '" + std::string(Member->name.GetString()) +
                        "': " + FilePath.generic_string());
                return false;
            }

            if (!Member->value.IsObject())
            {
                SetErrorMessage(
                    ErrorMessage,
                    std::string(Member->name.GetString()) + " must be an object: " + FilePath.generic_string());
                return false;
            }

            MaterialRendererBinding Binding;
            if (!ReadRendererBinding(Member->value, FilePath, Member->name.GetString(), Binding, ErrorMessage))
            {
                return false;
            }

            Parsed.Bindings.push_back(std::move(Binding));
        }

        if (Parsed.Bindings.empty())
        {
            SetErrorMessage(ErrorMessage, "Material description has no renderer binding: " + FilePath.generic_string());
            return false;
        }

        Description = std::move(Parsed);
        return true;
    }
}
