#pragma once

#include "Interface/Public/IRuntimeModule.hpp"

namespace ShadowEngine
{
    class GraphManager : public IRuntimeModule
    {
        public:
            GraphManager();
            ~GraphManager() override;
    };
}