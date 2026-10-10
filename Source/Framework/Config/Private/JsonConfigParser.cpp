#include "Framework/Config/Private/JsonConfigParser.h"

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
        void SetError(std::string* ErrorMessage, std::string Message)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = std::move(Message);
            }
        }

        bool ParseJsonFile(
            const std::filesystem::path& FilePath,
            rapidjson::Document& Document,
            std::string* ErrorMessage)
        {
            std::ifstream File(FilePath, std::ios::binary);
            if (!File)
            {
                SetError(
                    ErrorMessage,
                    "Unable to open config file: " + FilePath.string());
                return false;
            }

            const std::string Json{
                std::istreambuf_iterator<char>(File),
                std::istreambuf_iterator<char>()};

            Document.Parse(Json.data(), Json.size());
            if (Document.HasParseError())
            {
                std::ostringstream Message;
                Message << "Invalid JSON in " << FilePath.string()
                        << " at offset " << Document.GetErrorOffset() << ": "
                        << rapidjson::GetParseError_En(
                               Document.GetParseError());
                SetError(ErrorMessage, Message.str());
                return false;
            }

            if (!Document.IsObject())
            {
                SetError(ErrorMessage, "The JSON root must be an object");
                return false;
            }

            return true;
        }

        const rapidjson::Value* FindMember(
            const rapidjson::Value& Object,
            const char* Name,
            std::string* ErrorMessage)
        {
            const auto Member = Object.FindMember(Name);
            if (Member == Object.MemberEnd())
            {
                SetError(
                    ErrorMessage,
                    std::string("Missing JSON member: ") + Name);
                return nullptr;
            }

            return &Member->value;
        }

        bool ReadInt(
            const rapidjson::Value& Object,
            const char* Name,
            int& Result,
            std::string* ErrorMessage)
        {
            const rapidjson::Value* Value =
                FindMember(Object, Name, ErrorMessage);
            if (Value == nullptr)
            {
                return false;
            }

            if (!Value->IsInt())
            {
                SetError(
                    ErrorMessage,
                    std::string("JSON member must be an integer: ") + Name);
                return false;
            }

            Result = Value->GetInt();
            return true;
        }

        bool ReadFloat(
            const rapidjson::Value& Object,
            const char* Name,
            float& Result,
            std::string* ErrorMessage)
        {
            const rapidjson::Value* Value =
                FindMember(Object, Name, ErrorMessage);
            if (Value == nullptr)
            {
                return false;
            }

            if (!Value->IsNumber())
            {
                SetError(
                    ErrorMessage,
                    std::string("JSON member must be a number: ") + Name);
                return false;
            }

            Result = Value->GetFloat();
            return true;
        }

        bool ReadString(
            const rapidjson::Value& Object,
            const char* Name,
            std::string& Result,
            std::string* ErrorMessage)
        {
            const rapidjson::Value* Value =
                FindMember(Object, Name, ErrorMessage);
            if (Value == nullptr)
            {
                return false;
            }

            if (!Value->IsString())
            {
                SetError(
                    ErrorMessage,
                    std::string("JSON member must be a string: ") + Name);
                return false;
            }

            Result.assign(Value->GetString(), Value->GetStringLength());
            return true;
        }

        bool ReadBool(
            const rapidjson::Value& Object,
            const char* Name,
            bool& Result,
            std::string* ErrorMessage)
        {
            const rapidjson::Value* Value =
                FindMember(Object, Name, ErrorMessage);
            if (Value == nullptr)
            {
                return false;
            }

            if (!Value->IsBool())
            {
                SetError(
                    ErrorMessage,
                    std::string("JSON member must be a boolean: ") + Name);
                return false;
            }

            Result = Value->GetBool();
            return true;
        }

        const rapidjson::Value* FindObject(
            const rapidjson::Value& Object,
            const char* Name,
            std::string* ErrorMessage)
        {
            const rapidjson::Value* Value =
                FindMember(Object, Name, ErrorMessage);
            if (Value == nullptr)
            {
                return nullptr;
            }

            if (!Value->IsObject())
            {
                SetError(
                    ErrorMessage,
                    std::string("JSON member must be an object: ") + Name);
                return nullptr;
            }

            return Value;
        }

        bool ReadColor(
            const rapidjson::Value& Object,
            const char* Name,
            std::vector<float>& Result,
            std::string* ErrorMessage)
        {
            const rapidjson::Value* Value =
                FindMember(Object, Name, ErrorMessage);
            if (Value == nullptr)
            {
                return false;
            }

            if (!Value->IsArray() ||
                (Value->Size() != 3 && Value->Size() != 4))
            {
                SetError(
                    ErrorMessage,
                    std::string(
                        "JSON member must contain 3 or 4 numbers: ") +
                        Name);
                return false;
            }

            std::vector<float> Color;
            Color.reserve(Value->Size());
            for (const rapidjson::Value& Component : Value->GetArray())
            {
                if (!Component.IsNumber())
                {
                    SetError(
                        ErrorMessage,
                        std::string(
                            "JSON color contains a non-number: ") +
                            Name);
                    return false;
                }

                Color.push_back(Component.GetFloat());
            }

            Result = std::move(Color);
            return true;
        }

        bool ReadVec3(
            const rapidjson::Value& Object,
            const char* Name,
            std::vector<float>& Result,
            std::string* ErrorMessage)
        {
            const rapidjson::Value* Value =
                FindMember(Object, Name, ErrorMessage);
            if (Value == nullptr)
            {
                return false;
            }

            if (!Value->IsArray() || Value->Size() != 3)
            {
                SetError(
                    ErrorMessage,
                    std::string("JSON member must contain 3 numbers: ") +
                        Name);
                return false;
            }

            std::vector<float> Vector;
            Vector.reserve(3);
            for (const rapidjson::Value& Component : Value->GetArray())
            {
                if (!Component.IsNumber())
                {
                    SetError(
                        ErrorMessage,
                        std::string("JSON vector contains a non-number: ") +
                            Name);
                    return false;
                }

                Vector.push_back(Component.GetFloat());
            }

            Result = std::move(Vector);
            return true;
        }

        bool RequireMembers(
            const rapidjson::Value& Object,
            const char* const* Names,
            size_t Count,
            const char* Label,
            std::string* ErrorMessage)
        {
            if (Object.MemberCount() != Count)
            {
                SetError(ErrorMessage, std::string(Label) + " has an unexpected set of members");
                return false;
            }

            for (size_t Index = 0; Index < Count; ++Index)
            {
                if (!Object.HasMember(Names[Index]))
                {
                    SetError(ErrorMessage, std::string(Label) + " is missing member: " + Names[Index]);
                    return false;
                }
            }

            return true;
        }

        bool ReadSceneLight(
            const rapidjson::Value& Item,
            SceneSetting::Light& Light,
            std::string* ErrorMessage)
        {
            std::string Type;
            if (!ReadString(Item, "Type", Type, ErrorMessage))
            {
                return false;
            }

            if (Type == "DirectLight")
            {
                const char* const Members[] = {"Type", "Direction", "Color", "Intensity"};
                Light.Type = SceneSetting::LightType::Direct;
                return RequireMembers(Item, Members, 4, "DirectLight", ErrorMessage) &&
                       ReadVec3(Item, "Direction", Light.Direction, ErrorMessage) &&
                       ReadColor(Item, "Color", Light.Color, ErrorMessage) &&
                       ReadFloat(Item, "Intensity", Light.Intensity, ErrorMessage);
            }

            if (Type == "PointLight")
            {
                const char* const Members[] = {"Type", "Position", "Color", "Intensity", "Radius"};
                Light.Type = SceneSetting::LightType::Point;
                return RequireMembers(Item, Members, 5, "PointLight", ErrorMessage) &&
                       ReadVec3(Item, "Position", Light.Position, ErrorMessage) &&
                       ReadColor(Item, "Color", Light.Color, ErrorMessage) &&
                       ReadFloat(Item, "Intensity", Light.Intensity, ErrorMessage) &&
                       ReadFloat(Item, "Radius", Light.Radius, ErrorMessage);
            }

            if (Type == "SkyLight")
            {
                const char* const Members[] = {"Type", "Color", "Intensity"};
                Light.Type = SceneSetting::LightType::Sky;
                return RequireMembers(Item, Members, 3, "SkyLight", ErrorMessage) &&
                       ReadColor(Item, "Color", Light.Color, ErrorMessage) &&
                       ReadFloat(Item, "Intensity", Light.Intensity, ErrorMessage);
            }

            SetError(ErrorMessage, "Light Type must be DirectLight, PointLight, or SkyLight");
            return false;
        }

        const rapidjson::Value* FindArray(
            const rapidjson::Value& Object,
            const char* Name,
            std::string* ErrorMessage)
        {
            const rapidjson::Value* Value =
                FindMember(Object, Name, ErrorMessage);
            if (Value == nullptr)
            {
                return nullptr;
            }

            if (!Value->IsArray())
            {
                SetError(
                    ErrorMessage,
                    std::string("JSON member must be an array: ") + Name);
                return nullptr;
            }

            return Value;
        }
    }

    bool JsonConfigParser::LoadViewportSetting(
        const std::filesystem::path& FilePath,
        EngineSetting::ViewportSetting& Setting,
        std::string* ErrorMessage)
    {
        rapidjson::Document Document;
        if (!ParseJsonFile(FilePath, Document, ErrorMessage))
        {
            return false;
        }

        const rapidjson::Value* Viewport =
            FindMember(Document, "ViewportSetting", ErrorMessage);
        if (Viewport == nullptr)
        {
            return false;
        }

        if (!Viewport->IsObject())
        {
            SetError(
                ErrorMessage,
                "JSON member must be an object: ViewportSetting");
            return false;
        }

        EngineSetting::ViewportSetting ParsedSetting;
        if (!ReadInt(
                *Viewport,
                "Width",
                ParsedSetting.Width,
                ErrorMessage) ||
            !ReadInt(
                *Viewport,
                "Height",
                ParsedSetting.Height,
                ErrorMessage) ||
            !ReadInt(
                *Viewport,
                "AspectWidth",
                ParsedSetting.AspectWidth,
                ErrorMessage) ||
            !ReadInt(
                *Viewport,
                "AspectHeight",
                ParsedSetting.AspectHeight,
                ErrorMessage) ||
            !ReadFloat(
                *Viewport,
                "FOV",
                ParsedSetting.FOV,
                ErrorMessage) ||
            !ReadFloat(
                *Viewport,
                "NearPlane",
                ParsedSetting.NearPlane,
                ErrorMessage) ||
            !ReadFloat(
                *Viewport,
                "FarPlane",
                ParsedSetting.FarPlane,
                ErrorMessage) ||
            !ReadColor(
                *Viewport,
                "BackgroundColor",
                ParsedSetting.BackgroundColor,
                ErrorMessage) ||
            !ReadString(
                *Viewport,
                "ClearFlags",
                ParsedSetting.ClearFlags,
                ErrorMessage))
        {
            return false;
        }

        if (!ValidateViewportSetting(ParsedSetting, ErrorMessage))
        {
            return false;
        }

        Setting = std::move(ParsedSetting);
        return true;
    }

    bool JsonConfigParser::ValidateViewportSetting(
        const EngineSetting::ViewportSetting& Setting,
        std::string* ErrorMessage)
    {
        if (Setting.Width <= 0 ||
            Setting.Height <= 0 ||
            Setting.AspectWidth <= 0 ||
            Setting.AspectHeight <= 0)
        {
            SetError(
                ErrorMessage,
                "Viewport dimensions and aspect ratio must be positive");
            return false;
        }

        if (Setting.FOV <= 0.0F || Setting.FOV >= 180.0F)
        {
            SetError(
                ErrorMessage,
                "FOV must be between 0 and 180 degrees");
            return false;
        }

        if (Setting.NearPlane <= 0.0F ||
            Setting.FarPlane <= Setting.NearPlane)
        {
            SetError(
                ErrorMessage,
                "NearPlane must be positive and FarPlane must be greater");
            return false;
        }

        if (Setting.BackgroundColor.size() != 3 &&
            Setting.BackgroundColor.size() != 4)
        {
            SetError(
                ErrorMessage,
                "BackgroundColor must contain 3 or 4 components");
            return false;
        }

        if (Setting.ClearFlags.empty())
        {
            SetError(ErrorMessage, "ClearFlags cannot be empty");
            return false;
        }

        return true;
    }

    bool JsonConfigParser::LoadMovementSetting(
        const std::filesystem::path& FilePath,
        EngineSetting::MovementSetting& Setting,
        std::string* ErrorMessage)
    {
        rapidjson::Document Document;
        if (!ParseJsonFile(FilePath, Document, ErrorMessage))
        {
            return false;
        }

        const rapidjson::Value* Movement =
            FindMember(Document, "MovementSetting", ErrorMessage);
        if (Movement == nullptr)
        {
            return false;
        }

        if (!Movement->IsObject())
        {
            SetError(
                ErrorMessage,
                "JSON member must be an object: MovementSetting");
            return false;
        }

        EngineSetting::MovementSetting ParsedSetting;
        if (!ReadFloat(
                *Movement,
                "ForwardSpeed",
                ParsedSetting.ForwardSpeed,
                ErrorMessage) ||
            !ReadFloat(
                *Movement,
                "RightSpeed",
                ParsedSetting.RightSpeed,
                ErrorMessage) ||
            !ReadFloat(
                *Movement,
                "UpSpeed",
                ParsedSetting.UpSpeed,
                ErrorMessage) ||
            !ReadFloat(
                *Movement,
                "DownSpeed",
                ParsedSetting.DownSpeed,
                ErrorMessage) ||
            !ReadFloat(
                *Movement,
                "YawSpeed",
                ParsedSetting.YawSpeed,
                ErrorMessage) ||
            !ReadFloat(
                *Movement,
                "PitchSpeed",
                ParsedSetting.PitchSpeed,
                ErrorMessage))
        {
            return false;
        }

        if (!ValidateMovementSetting(ParsedSetting, ErrorMessage))
        {
            return false;
        }

        Setting = ParsedSetting;
        return true;
    }

    bool JsonConfigParser::ValidateMovementSetting(
        const EngineSetting::MovementSetting& Setting,
        std::string* ErrorMessage)
    {
        if (Setting.ForwardSpeed <= 0.0F ||
            Setting.RightSpeed <= 0.0F ||
            Setting.UpSpeed <= 0.0F ||
            Setting.DownSpeed <= 0.0F ||
            Setting.YawSpeed <= 0.0F ||
            Setting.PitchSpeed <= 0.0F)
        {
            SetError(ErrorMessage, "Movement speeds must be positive");
            return false;
        }

        return true;
    }

    bool JsonConfigParser::LoadRHISetting(
        const std::filesystem::path& FilePath,
        RenderSetting::RHISetting& Setting,
        std::string* ErrorMessage)
    {
        rapidjson::Document Document;
        if (!ParseJsonFile(FilePath, Document, ErrorMessage))
        {
            return false;
        }

        const rapidjson::Value* RHI =
            FindObject(Document, "RHISetting", ErrorMessage);
        if (RHI == nullptr)
        {
            return false;
        }

        RenderSetting::RHISetting ParsedSetting;
        if (!ReadString(
                Document,
                "Renderer",
                ParsedSetting.Renderer,
                ErrorMessage) ||
            !ReadString(
                *RHI,
                "Backend",
                ParsedSetting.Backend,
                ErrorMessage) ||
            !ReadBool(
                *RHI,
                "VSync",
                ParsedSetting.bVSync,
                ErrorMessage) ||
            !ReadBool(
                *RHI,
                "DebugLayer",
                ParsedSetting.bDebugLayer,
                ErrorMessage) ||
            !ReadInt(
                *RHI,
                "BackBufferCount",
                ParsedSetting.BackBufferCount,
                ErrorMessage))
        {
            return false;
        }

        if (!ValidateRHISetting(ParsedSetting, ErrorMessage))
        {
            return false;
        }

        Setting = std::move(ParsedSetting);
        return true;
    }

    bool JsonConfigParser::ValidateRHISetting(
        const RenderSetting::RHISetting& Setting,
        std::string* ErrorMessage)
    {
        if (Setting.Renderer != "BlinnPhongRenderer" && Setting.Renderer != "DebugRenderer" &&
            Setting.Renderer != "PBRForwardRenderer")
        {
            SetError(ErrorMessage, "Renderer must be BlinnPhongRenderer, DebugRenderer, or PBRForwardRenderer");
            return false;
        }

        if (Setting.Backend.empty())
        {
            SetError(ErrorMessage, "RHI Backend cannot be empty");
            return false;
        }

        if (Setting.BackBufferCount < 2 || Setting.BackBufferCount > 8)
        {
            SetError(
                ErrorMessage,
                "BackBufferCount must be between 2 and 8");
            return false;
        }

        return true;
    }

    bool JsonConfigParser::LoadSceneSetting(
        const std::filesystem::path& FilePath,
        SceneSetting::Setting& Setting,
        std::string* ErrorMessage)
    {
        rapidjson::Document Document;
        if (!ParseJsonFile(FilePath, Document, ErrorMessage))
        {
            return false;
        }

        const rapidjson::Value* Meshes =
            FindObject(Document, "Mesh", ErrorMessage);
        if (Meshes == nullptr)
        {
            return false;
        }

        const rapidjson::Value* Lights =
            FindArray(Document, "Light", ErrorMessage);
        if (Lights == nullptr)
        {
            return false;
        }

        SceneSetting::Setting ParsedSetting;
        for (auto Member = Meshes->MemberBegin(); Member != Meshes->MemberEnd(); ++Member)
        {
            const std::string MeshPath(Member->name.GetString(), Member->name.GetStringLength());
            if (!Member->value.IsArray())
            {
                SetError(
                    ErrorMessage,
                    "Mesh instances must be an array: " + MeshPath);
                return false;
            }

            SceneSetting::Mesh Mesh;
            Mesh.Path = MeshPath;
            Mesh.Instances.reserve(Member->value.Size());
            for (const rapidjson::Value& Item : Member->value.GetArray())
            {
                if (!Item.IsObject())
                {
                    SetError(ErrorMessage, "Each mesh instance must be an object: " + MeshPath);
                    return false;
                }

                SceneSetting::MeshInstance Instance;
                const rapidjson::Value* TransformValue =
                    FindObject(Item, "Transform", ErrorMessage);
                if (TransformValue == nullptr ||
                    !ReadVec3(*TransformValue, "Translation", Instance.Transform.Translation, ErrorMessage) ||
                    !ReadVec3(*TransformValue, "Rotation", Instance.Transform.Rotation, ErrorMessage) ||
                    !ReadVec3(*TransformValue, "Scale", Instance.Transform.Scale, ErrorMessage))
                {
                    return false;
                }

                Mesh.Instances.push_back(std::move(Instance));
            }

            ParsedSetting.Meshes.push_back(std::move(Mesh));
        }

        ParsedSetting.Lights.reserve(Lights->Size());
        for (const rapidjson::Value& Item : Lights->GetArray())
        {
            if (!Item.IsObject())
            {
                SetError(ErrorMessage, "Each Light entry must be an object");
                return false;
            }

            SceneSetting::Light Light;
            if (!ReadSceneLight(Item, Light, ErrorMessage))
            {
                return false;
            }

            ParsedSetting.Lights.push_back(std::move(Light));
        }

        if (!ValidateSceneSetting(ParsedSetting, ErrorMessage))
        {
            return false;
        }

        Setting = std::move(ParsedSetting);
        return true;
    }

    bool JsonConfigParser::ValidateSceneSetting(
        const SceneSetting::Setting& Setting,
        std::string* ErrorMessage)
    {
        for (const SceneSetting::Mesh& Mesh : Setting.Meshes)
        {
            if (Mesh.Path.empty())
            {
                SetError(ErrorMessage, "Mesh path cannot be empty");
                return false;
            }

            if (Mesh.Instances.empty())
            {
                SetError(ErrorMessage, "Mesh has no instances: " + Mesh.Path);
                return false;
            }

            for (const SceneSetting::MeshInstance& Instance : Mesh.Instances)
            {
                if (Instance.Transform.Translation.size() != 3 ||
                    Instance.Transform.Rotation.size() != 3 ||
                    Instance.Transform.Scale.size() != 3)
                {
                    SetError(ErrorMessage, "Mesh Transform must contain translation, rotation, and scale: " + Mesh.Path);
                    return false;
                }
            }
        }

        for (const SceneSetting::Light& Light : Setting.Lights)
        {
            if (Light.Color.size() != 3 && Light.Color.size() != 4)
            {
                SetError(ErrorMessage, "Light Color must contain 3 or 4 components");
                return false;
            }

            if (Light.Intensity < 0.0F)
            {
                SetError(ErrorMessage, "Light Intensity cannot be negative");
                return false;
            }

            if (Light.Type == SceneSetting::LightType::Direct)
            {
                if (Light.Direction.size() != 3)
                {
                    SetError(ErrorMessage, "DirectLight Direction must contain 3 numbers");
                    return false;
                }

                const float Length =
                    Light.Direction[0] * Light.Direction[0] +
                    Light.Direction[1] * Light.Direction[1] +
                    Light.Direction[2] * Light.Direction[2];
                if (Length <= 0.0F)
                {
                    SetError(ErrorMessage, "DirectLight Direction cannot be zero");
                    return false;
                }
            }
            else if (Light.Type == SceneSetting::LightType::Point)
            {
                if (Light.Position.size() != 3)
                {
                    SetError(ErrorMessage, "PointLight Position must contain 3 numbers");
                    return false;
                }

                if (Light.Radius <= 0.0F)
                {
                    SetError(ErrorMessage, "PointLight Radius must be positive");
                    return false;
                }
            }
        }

        return true;
    }

    bool JsonConfigParser::LoadGlobalTextureSetting(
        const std::filesystem::path& FilePath,
        GlobalTextureSetting::Setting& Setting,
        std::string* ErrorMessage)
    {
        rapidjson::Document Document;
        if (!ParseJsonFile(FilePath, Document, ErrorMessage))
        {
            return false;
        }

        if (Document.MemberCount() != 1 || !Document.HasMember("EnvironmentMap"))
        {
            SetError(ErrorMessage, "Global texture file must contain EnvironmentMap");
            return false;
        }

        GlobalTextureSetting::Setting ParsedSetting;
        if (!ReadString(Document, "EnvironmentMap", ParsedSetting.EnvironmentMap, ErrorMessage))
        {
            return false;
        }

        if (ParsedSetting.EnvironmentMap.empty())
        {
            SetError(ErrorMessage, "EnvironmentMap cannot be empty");
            return false;
        }

        Setting = std::move(ParsedSetting);
        return true;
    }
}
