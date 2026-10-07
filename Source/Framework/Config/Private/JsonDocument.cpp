#include "Framework/Config/Private/JsonDocument.h"

#include "Framework/Common/Public/Log.h"

#include <fstream>
#include <iterator>
#include <sstream>

#include <rapidjson/error/en.h>

namespace ShadowEngine
{
    bool ParseJsonObject(
        const std::filesystem::path& FilePath,
        const char* Kind,
        rapidjson::Document& Document,
        std::string* ErrorMessage)
    {
        std::ifstream File(FilePath, std::ios::binary);
        if (!File)
        {
            SetErrorMessage(ErrorMessage, std::string("Unable to open ") + Kind + ": " + FilePath.generic_string());
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
            SetErrorMessage(ErrorMessage, std::string(Kind) + " must be an object: " + FilePath.generic_string());
            return false;
        }

        return true;
    }

    bool RequireMembers(
        const rapidjson::Value& Object,
        const std::filesystem::path& FilePath,
        const char* Kind,
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
                    std::string(Kind) + " has unsupported member '" + Member->name.GetString() +
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
                    std::string(Kind) + " is missing '" + Names[Index] + "': " + FilePath.generic_string());
                return false;
            }
        }

        return true;
    }
}
