#include "Framework/Scene/Public/Entity.h"

namespace ShadowEngine
{
    void Entity::SetMesh(const MeshAsset* InMesh)
    {
        Mesh = InMesh;
    }

    const MeshAsset* Entity::GetMesh() const
    {
        return Mesh;
    }

    Transform& Entity::GetTransform()
    {
        return LocalTransform;
    }

    const Transform& Entity::GetTransform() const
    {
        return LocalTransform;
    }
}
