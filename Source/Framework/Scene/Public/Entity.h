#pragma once

#include "Framework/Scene/Public/Transform.h"

namespace ShadowEngine
{
    class Entity final
    {
        public:
            [[nodiscard]] Transform& GetTransform();
            [[nodiscard]] const Transform& GetTransform() const;

        private:
            Transform LocalTransform;
    };
}
