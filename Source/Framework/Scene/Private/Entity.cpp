#include "Framework/Scene/Public/Entity.h"

namespace ShadowEngine
{
    Transform& Entity::GetTransform()
    {
        return LocalTransform;
    }

    const Transform& Entity::GetTransform() const
    {
        return LocalTransform;
    }
}
