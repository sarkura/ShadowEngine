#include "Framework/Config/Public/DescriptionParser.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/Config/Private/JsonDocument.h"

#include <cctype>
#include <string>
#include <string_view>
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
            const bool bForward = std::string_view(RendererName) == "ForwardRenderer";
            const char* AllowedMembers[7] = {"Vertex", "Fragment", "Material", "Light", "Parameters", nullptr, nullptr};
            uint32 AllowedCount = 5;
            if (bForward)
            {
                AllowedMembers[5] = "Surface";
                AllowedMembers[6] = "TwoSided";
                AllowedCount = 7;
            }
            for (auto Member = Object.MemberBegin(); Member != Object.MemberEnd(); ++Member)
            {
                bool bKnown = false;
                for (uint32 Index = 0; Index < AllowedCount; ++Index)
                {
                    if (Member->name == AllowedMembers[Index])
                    {
                        bKnown = true;
                        break;
                    }
                }
                if (!bKnown)
                {
                    SetErrorMessage(
                        ErrorMessage,
                        std::string(RendererName) + " has unsupported member '" + Member->name.GetString() +
                            "': " + FilePath.generic_string());
                    return false;
                }
            }

            const char* RequiredMembers[6] = {"Vertex", "Fragment", "Material", "Parameters", nullptr, nullptr};
            uint32 RequiredCount = 4;
            if (bForward)
            {
                RequiredMembers[4] = "Surface";
                RequiredMembers[5] = "TwoSided";
                RequiredCount = 6;
            }
            for (uint32 Index = 0; Index < RequiredCount; ++Index)
            {
                const char* Name = RequiredMembers[Index];
                if (!Object.HasMember(Name))
                {
                    SetErrorMessage(
                        ErrorMessage,
                        std::string(RendererName) + " is missing '" + Name + "': " + FilePath.generic_string());
                    return false;
                }
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
            if (Object.HasMember("Light"))
            {
                const rapidjson::Value& LightShader = Object["Light"];
                if (!LightShader.IsString() || LightShader.GetStringLength() == 0)
                {
                    SetErrorMessage(ErrorMessage, std::string(RendererName) + " Light must be a path: " + FilePath.generic_string());
                    return false;
                }

                Binding.LightShader = LightShader.GetString();
            }
            if (bForward)
            {
                const rapidjson::Value& Surface = Object["Surface"];
                if (!Surface.IsString())
                {
                    SetErrorMessage(ErrorMessage, "ForwardRenderer Surface must be a string: " + FilePath.generic_string());
                    return false;
                }

                const std::string_view SurfaceName = Surface.GetString();
                if (SurfaceName == "Opaque")
                {
                    Binding.Surface = EMaterialSurface::Opaque;
                }
                else if (SurfaceName == "Cutout")
                {
                    Binding.Surface = EMaterialSurface::Cutout;
                }
                else if (SurfaceName == "Translucent")
                {
                    Binding.Surface = EMaterialSurface::Translucent;
                }
                else
                {
                    SetErrorMessage(
                        ErrorMessage,
                        "ForwardRenderer Surface must be Opaque, Cutout, or Translucent: " + FilePath.generic_string());
                    return false;
                }

                const rapidjson::Value& TwoSided = Object["TwoSided"];
                if (!TwoSided.IsBool())
                {
                    SetErrorMessage(ErrorMessage, "ForwardRenderer TwoSided must be a boolean: " + FilePath.generic_string());
                    return false;
                }

                Binding.bTwoSided = TwoSided.GetBool();
            }
            if (Parameters.MemberCount() == 0)
            {
                return true;
            }

            const auto IsParameterName = [](std::string_view Name) -> bool
            {
                if (Name.empty() || Name == "Sampler" || Name.rfind("Padding", 0) == 0)
                {
                    return false;
                }

                if (std::isalpha(static_cast<unsigned char>(Name.front())) == 0 && Name.front() != '_')
                {
                    return false;
                }

                for (const char Character : Name)
                {
                    if (std::isalnum(static_cast<unsigned char>(Character)) == 0 && Character != '_')
                    {
                        return false;
                    }
                }

                return true;
            };
            const auto ReadComponents = [&](const std::string& Name, const rapidjson::Value& Value, uint32 Count, float* Out) -> bool
            {
                if (!Value.IsArray() || Value.Size() != Count)
                {
                    SetErrorMessage(
                        ErrorMessage,
                        "Material parameter '" + Name + "' must contain " + std::to_string(Count) +
                            " numbers: " + FilePath.generic_string());
                    return false;
                }

                for (rapidjson::SizeType Index = 0; Index < Count; ++Index)
                {
                    if (!Value[Index].IsNumber())
                    {
                        SetErrorMessage(
                            ErrorMessage,
                            "Material parameter '" + Name + "' must contain " + std::to_string(Count) +
                                " numbers: " + FilePath.generic_string());
                        return false;
                    }

                    Out[Index] = Value[Index].GetFloat();
                }

                return true;
            };
            for (auto Member = Parameters.MemberBegin(); Member != Parameters.MemberEnd(); ++Member)
            {
                const std::string Name = Member->name.GetString();
                if (!IsParameterName(Name))
                {
                    SetErrorMessage(
                        ErrorMessage,
                        "Material parameter '" + Name + "' is not a valid name: " + FilePath.generic_string());
                    return false;
                }

                if (!Member->value.IsObject() || Member->value.MemberCount() != 1)
                {
                    SetErrorMessage(
                        ErrorMessage,
                        "Material parameter '" + Name + "' must declare one type: " + FilePath.generic_string());
                    return false;
                }

                const auto TypeMember = Member->value.MemberBegin();
                const std::string TypeName = TypeMember->name.GetString();
                if (TypeName == "texture")
                {
                    if (!TypeMember->value.IsString() || TypeMember->value.GetStringLength() == 0)
                    {
                        SetErrorMessage(
                            ErrorMessage,
                            "Material parameter '" + Name + "' texture must be a path: " + FilePath.generic_string());
                        return false;
                    }

                    Binding.Textures.emplace_back(
                        Name,
                        std::string(TypeMember->value.GetString(), TypeMember->value.GetStringLength()));
                    continue;
                }

                MaterialScalarParameter Scalar;
                Scalar.Name = Name;
                if (TypeName == "float")
                {
                    if (!TypeMember->value.IsNumber())
                    {
                        SetErrorMessage(
                            ErrorMessage,
                            "Material parameter '" + Name + "' float must be a number: " + FilePath.generic_string());
                        return false;
                    }

                    Scalar.Components = 1;
                    Scalar.Value[0] = TypeMember->value.GetFloat();
                }
                else if (TypeName == "float2")
                {
                    Scalar.Components = 2;
                    if (!ReadComponents(Name, TypeMember->value, 2, Scalar.Value))
                    {
                        return false;
                    }
                }
                else if (TypeName == "float3")
                {
                    Scalar.Components = 3;
                    if (!ReadComponents(Name, TypeMember->value, 3, Scalar.Value))
                    {
                        return false;
                    }
                }
                else if (TypeName == "float4")
                {
                    Scalar.Components = 4;
                    if (!ReadComponents(Name, TypeMember->value, 4, Scalar.Value))
                    {
                        return false;
                    }
                }
                else
                {
                    SetErrorMessage(
                        ErrorMessage,
                        "Material parameter '" + Name + "' has unsupported type '" + TypeName + "': " +
                            FilePath.generic_string());
                    return false;
                }

                Binding.Scalars.push_back(Scalar);
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

        const char* const RendererNames[] = {"BlinnPhongRenderer", "DebugRenderer", "ForwardRenderer"};
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

    bool DescriptionParser::LoadTextureDescription(
        const std::filesystem::path& FilePath,
        TextureDescription& Description,
        std::string* ErrorMessage)
    {
        rapidjson::Document Document;
        if (!ParseJsonObject(FilePath, "Texture description", Document, ErrorMessage))
        {
            return false;
        }

        const char* const Members[] = {"Type", "Source", "Encoding"};
        if (!RequireMembers(Document, FilePath, "Texture description", Members, 3, ErrorMessage))
        {
            return false;
        }

        const rapidjson::Value& Type = Document["Type"];
        if (!Type.IsString() || std::string(Type.GetString()) != "Texture")
        {
            SetErrorMessage(ErrorMessage, "Texture description Type must be Texture: " + FilePath.generic_string());
            return false;
        }

        const rapidjson::Value& Source = Document["Source"];
        if (!Source.IsString() || Source.GetStringLength() == 0)
        {
            SetErrorMessage(ErrorMessage, "Texture description Source must be a path: " + FilePath.generic_string());
            return false;
        }

        const rapidjson::Value& Encoding = Document["Encoding"];
        if (!Encoding.IsString())
        {
            SetErrorMessage(ErrorMessage, "Texture description Encoding must be sRGB or Linear: " + FilePath.generic_string());
            return false;
        }

        const std::string EncodingName = Encoding.GetString();
        TextureDescription Parsed;
        Parsed.Source = Source.GetString();
        if (EncodingName == "sRGB")
        {
            Parsed.Encoding = ETextureEncoding::sRGB;
        }
        else if (EncodingName == "Linear")
        {
            Parsed.Encoding = ETextureEncoding::Linear;
        }
        else
        {
            SetErrorMessage(ErrorMessage, "Texture description Encoding must be sRGB or Linear: " + FilePath.generic_string());
            return false;
        }

        Description = std::move(Parsed);
        return true;
    }
}
