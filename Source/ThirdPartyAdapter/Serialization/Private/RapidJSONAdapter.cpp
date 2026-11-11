#include "ThirdPartyAdapter/Serialization/Public/RapidJSONAdapter.h"

#include <cstring>
#include <sstream>
#include <utility>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

namespace ShadowEngine
{
    namespace
    {
        const rapidjson::Value* AsValue(const void* Data)
        {
            return static_cast<const rapidjson::Value*>(Data);
        }

        void SetError(std::string* ErrorMessage, std::string Message)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = std::move(Message);
            }
        }
    }

    struct RapidJSONDocument::Storage
    {
        rapidjson::Document Document;
    };

    RapidJSONValue::RapidJSONValue(const void* Value)
        : Data(Value)
    {
    }

    bool RapidJSONValue::IsValid() const
    {
        return Data != nullptr;
    }

    bool RapidJSONValue::IsObject() const
    {
        return Data != nullptr && AsValue(Data)->IsObject();
    }

    bool RapidJSONValue::IsArray() const
    {
        return Data != nullptr && AsValue(Data)->IsArray();
    }

    bool RapidJSONValue::IsString() const
    {
        return Data != nullptr && AsValue(Data)->IsString();
    }

    bool RapidJSONValue::IsNumber() const
    {
        return Data != nullptr && AsValue(Data)->IsNumber();
    }

    bool RapidJSONValue::IsBool() const
    {
        return Data != nullptr && AsValue(Data)->IsBool();
    }

    bool RapidJSONValue::IsInt() const
    {
        return Data != nullptr && AsValue(Data)->IsInt();
    }

    bool RapidJSONValue::IsUint() const
    {
        return Data != nullptr && AsValue(Data)->IsUint();
    }

    const char* RapidJSONValue::GetString() const
    {
        return AsValue(Data)->GetString();
    }

    size_t RapidJSONValue::GetStringLength() const
    {
        return AsValue(Data)->GetStringLength();
    }

    bool RapidJSONValue::GetBool() const
    {
        return AsValue(Data)->GetBool();
    }

    float RapidJSONValue::GetFloat() const
    {
        return AsValue(Data)->GetFloat();
    }

    int RapidJSONValue::GetInt() const
    {
        return AsValue(Data)->GetInt();
    }

    unsigned RapidJSONValue::GetUint() const
    {
        return AsValue(Data)->GetUint();
    }

    size_t RapidJSONValue::Size() const
    {
        return AsValue(Data)->Size();
    }

    size_t RapidJSONValue::MemberCount() const
    {
        return AsValue(Data)->MemberCount();
    }

    bool RapidJSONValue::HasMember(const char* Name) const
    {
        return AsValue(Data)->HasMember(Name);
    }

    bool RapidJSONValue::NameEquals(const char* Name) const
    {
        return AsValue(Data)->IsString() && std::strcmp(AsValue(Data)->GetString(), Name) == 0;
    }

    RapidJSONValue RapidJSONValue::operator[](const char* Name) const
    {
        return RapidJSONValue(&(*AsValue(Data))[Name]);
    }

    RapidJSONValue RapidJSONValue::operator[](size_t Index) const
    {
        return RapidJSONValue(&(*AsValue(Data))[static_cast<rapidjson::SizeType>(Index)]);
    }

    RapidJSONValue RapidJSONValue::Find(const char* Name) const
    {
        const auto Member = AsValue(Data)->FindMember(Name);
        if (Member == AsValue(Data)->MemberEnd())
        {
            return {};
        }

        return RapidJSONValue(&Member->value);
    }

    RapidJSONValue RapidJSONValue::MemberName(size_t Index) const
    {
        return RapidJSONValue(&(AsValue(Data)->MemberBegin() + static_cast<rapidjson::SizeType>(Index))->name);
    }

    RapidJSONValue RapidJSONValue::MemberValue(size_t Index) const
    {
        return RapidJSONValue(&(AsValue(Data)->MemberBegin() + static_cast<rapidjson::SizeType>(Index))->value);
    }

    bool RapidJSONValue::operator==(std::nullptr_t) const
    {
        return Data == nullptr;
    }

    bool RapidJSONValue::operator!=(std::nullptr_t) const
    {
        return Data != nullptr;
    }

    RapidJSONDocument::RapidJSONDocument()
        : Data(new Storage)
    {
    }

    RapidJSONDocument::~RapidJSONDocument()
    {
        delete Data;
    }

    RapidJSONDocument::RapidJSONDocument(RapidJSONDocument&& Other) noexcept
        : Data(Other.Data)
    {
        Other.Data = nullptr;
    }

    RapidJSONDocument& RapidJSONDocument::operator=(RapidJSONDocument&& Other) noexcept
    {
        if (this != &Other)
        {
            delete Data;
            Data = Other.Data;
            Other.Data = nullptr;
        }

        return *this;
    }

    bool RapidJSONDocument::Parse(const char* Json, size_t Size, std::string* ErrorMessage)
    {
        Data->Document.Parse(Json, Size);
        if (Data->Document.HasParseError())
        {
            std::ostringstream Message;
            Message << "at offset " << Data->Document.GetErrorOffset() << ": "
                    << rapidjson::GetParseError_En(Data->Document.GetParseError());
            SetError(ErrorMessage, Message.str());
            return false;
        }

        return true;
    }

    bool RapidJSONDocument::IsObject() const
    {
        return Data->Document.IsObject();
    }

    bool RapidJSONDocument::HasMember(const char* Name) const
    {
        return Data->Document.HasMember(Name);
    }

    size_t RapidJSONDocument::MemberCount() const
    {
        return Data->Document.MemberCount();
    }

    RapidJSONValue RapidJSONDocument::Root() const
    {
        return RapidJSONValue(&Data->Document);
    }

    RapidJSONValue RapidJSONDocument::operator[](const char* Name) const
    {
        return RapidJSONValue(&Data->Document[Name]);
    }

    RapidJSONValue RapidJSONDocument::MemberName(size_t Index) const
    {
        return RapidJSONValue(&(Data->Document.MemberBegin() + static_cast<rapidjson::SizeType>(Index))->name);
    }

    RapidJSONValue RapidJSONDocument::MemberValue(size_t Index) const
    {
        return RapidJSONValue(&(Data->Document.MemberBegin() + static_cast<rapidjson::SizeType>(Index))->value);
    }

    RapidJSONDocument::operator RapidJSONValue() const
    {
        return Root();
    }
}
