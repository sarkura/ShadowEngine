#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Scene/Public/Entity.h"

#include <deque>

namespace ShadowEngine
{
    class Scene final : public NonCopyable
    {
        public:
            Entity& CreateEntity();

            [[nodiscard]] const std::deque<Entity>& GetEntities() const;

        private:
            std::deque<Entity> Entities;
    };
}
