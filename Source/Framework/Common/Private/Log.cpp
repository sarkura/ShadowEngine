#include "Framework/Common/Public/Log.h"

#include <iostream>
#include <mutex>

namespace ShadowEngine
{
    namespace
    {
        std::mutex LogMutex;

        std::string_view GetLevelName(ELogLevel Level)
        {
            switch (Level)
            {
                case ELogLevel::Info:
                    return "Info";
                case ELogLevel::Warning:
                    return "Warning";
                case ELogLevel::Error:
                    return "Error";
            }

            return "Unknown";
        }
    }

    void Log::Write(ELogLevel Level, std::string_view Message)
    {
        const std::scoped_lock Lock(LogMutex);
        std::ostream& Stream = Level == ELogLevel::Info ? std::cout : std::cerr;
        Stream << "[" << GetLevelName(Level) << "] " << Message << std::endl;
    }

    void SetErrorMessage(std::string* ErrorMessage, std::string Message)
    {
        if (ErrorMessage != nullptr)
        {
            *ErrorMessage = std::move(Message);
        }
    }
}
