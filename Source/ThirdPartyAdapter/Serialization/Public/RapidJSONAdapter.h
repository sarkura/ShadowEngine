#pragma once

#include <cstddef>
#include <string>

namespace ShadowEngine
{
    class RapidJSONValue
    {
        public:
            RapidJSONValue() = default;

            [[nodiscard]] bool IsValid() const;
            [[nodiscard]] bool IsObject() const;
            [[nodiscard]] bool IsArray() const;
            [[nodiscard]] bool IsString() const;
            [[nodiscard]] bool IsNumber() const;
            [[nodiscard]] bool IsBool() const;
            [[nodiscard]] bool IsInt() const;
            [[nodiscard]] bool IsUint() const;

            [[nodiscard]] const char* GetString() const;
            [[nodiscard]] size_t GetStringLength() const;
            [[nodiscard]] bool GetBool() const;
            [[nodiscard]] float GetFloat() const;
            [[nodiscard]] int GetInt() const;
            [[nodiscard]] unsigned GetUint() const;
            [[nodiscard]] size_t Size() const;
            [[nodiscard]] size_t MemberCount() const;
            [[nodiscard]] bool HasMember(const char* Name) const;
            [[nodiscard]] bool NameEquals(const char* Name) const;

            [[nodiscard]] RapidJSONValue operator[](const char* Name) const;
            [[nodiscard]] RapidJSONValue operator[](size_t Index) const;
            [[nodiscard]] RapidJSONValue Find(const char* Name) const;
            [[nodiscard]] RapidJSONValue MemberName(size_t Index) const;
            [[nodiscard]] RapidJSONValue MemberValue(size_t Index) const;
            [[nodiscard]] bool operator==(std::nullptr_t) const;
            [[nodiscard]] bool operator!=(std::nullptr_t) const;

        private:
            friend class RapidJSONDocument;

            explicit RapidJSONValue(const void* Value);

            const void* Data = nullptr;
    };

    class RapidJSONDocument
    {
        public:
            RapidJSONDocument();
            ~RapidJSONDocument();

            RapidJSONDocument(const RapidJSONDocument&) = delete;
            RapidJSONDocument& operator=(const RapidJSONDocument&) = delete;
            RapidJSONDocument(RapidJSONDocument&&) noexcept;
            RapidJSONDocument& operator=(RapidJSONDocument&&) noexcept;

            [[nodiscard]] bool Parse(const char* Json, size_t Size, std::string* ErrorMessage);
            [[nodiscard]] bool IsObject() const;
            [[nodiscard]] bool HasMember(const char* Name) const;
            [[nodiscard]] size_t MemberCount() const;
            [[nodiscard]] RapidJSONValue Root() const;
            [[nodiscard]] RapidJSONValue operator[](const char* Name) const;
            [[nodiscard]] RapidJSONValue MemberName(size_t Index) const;
            [[nodiscard]] RapidJSONValue MemberValue(size_t Index) const;
            operator RapidJSONValue() const;

        private:
            struct Storage;
            Storage* Data = nullptr;
    };
}
