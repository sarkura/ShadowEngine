#include "ThirdPartyAdapter/Serialization/Public/RapidJSONAdapter.h"
#include "Framework/Config/Private/JsonDocument.h"

#include "Framework/Common/Public/Log.h"

#include <fstream>
#include <iterator>
#include <sstream>


namespace ShadowEngine
{
    bool ParseJsonObject(
        const std::filesystem::path& FilePath,
        const char* Kind,
        RapidJSONDocument& Document,
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

        std::string ParseError;
        if (!Document.Parse(Json.data(), Json.size(), &ParseError))
        {
            SetErrorMessage(ErrorMessage, std::string("Invalid JSON in ") + FilePath.generic_string() + " " + ParseError);
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
        const RapidJSONValue& Object,
        const std::filesystem::path& FilePath,
        const char* Kind,
        const char* const* Names,
        size_t NameCount,
        std::string* ErrorMessage)
    {
        for (size_t MemberIndex = 0; MemberIndex < Object.MemberCount(); ++MemberIndex)
        {
            const RapidJSONValue Name = Object.MemberName(MemberIndex);
            bool bKnown = false;
            for (size_t Index = 0; Index < NameCount; ++Index)
            {
                if (Name.NameEquals(Names[Index]))
                {
                    bKnown = true;
                    break;
                }
            }
            if (!bKnown)
            {
                SetErrorMessage(
                    ErrorMessage,
                    std::string(Kind) + " has unsupported member '" + Name.GetString() +
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
