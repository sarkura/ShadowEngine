#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Common/Public/Types.h"

#include <string>

namespace ShadowEngine
{
    struct RHIAdapterInfo
    {
        std::string Name;
        uint64 DedicatedVideoMemory = 0;
    };

    class RHIAdapter : public NonCopyable
    {
        public:
            virtual ~RHIAdapter() = default;

            [[nodiscard]] virtual const RHIAdapterInfo& GetInfo() const = 0;
    };
}
