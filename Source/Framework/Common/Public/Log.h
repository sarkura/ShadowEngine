#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace ShadowEngine
{
    enum class ELogLevel
    {
        Info,
        Warning,
        Error
    };

    class Log final
    {
        public:
            Log() = delete;

            static void Write(ELogLevel Level, std::string_view Message);

            template <typename... ArgTypes>
            static void Info(std::format_string<ArgTypes...> Format, ArgTypes&&... Args)
            {
                Write(ELogLevel::Info, std::format(Format, std::forward<ArgTypes>(Args)...));
            }

            template <typename... ArgTypes>
            static void Warning(std::format_string<ArgTypes...> Format, ArgTypes&&... Args)
            {
                Write(ELogLevel::Warning, std::format(Format, std::forward<ArgTypes>(Args)...));
            }

            template <typename... ArgTypes>
            static void Error(std::format_string<ArgTypes...> Format, ArgTypes&&... Args)
            {
                Write(ELogLevel::Error, std::format(Format, std::forward<ArgTypes>(Args)...));
            }
    };

    void SetErrorMessage(std::string* ErrorMessage, std::string Message);
}
