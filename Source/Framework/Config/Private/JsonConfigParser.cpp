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
}
