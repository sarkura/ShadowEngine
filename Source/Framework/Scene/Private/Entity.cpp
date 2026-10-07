#include "Framework/Scene/Public/Entity.h"

#include <utility>

namespace ShadowEngine
{
    void Entity::SetMesh(MeshHandle InMesh, std::vector<MaterialInstanceHandle> InMaterials)
    {
        Mesh = InMesh;
        Materials = std::move(InMaterials);
    }

    MeshHandle Entity::GetMesh() const
    {
        return Mesh;
    }

    const std::vector<MaterialInstanceHandle>& Entity::GetMaterials() const
    {
        return Materials;
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
