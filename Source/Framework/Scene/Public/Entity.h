#pragma once

#include "Framework/Asset/Public/AssetHandle.h"
#include "Framework/Material/Public/MaterialHandle.h"
#include "Framework/Scene/Public/Transform.h"

#include <vector>

namespace ShadowEngine
{
    class Entity final
    {
        public:
            void SetMesh(MeshHandle InMesh, std::vector<MaterialInstanceHandle> InMaterials);

            [[nodiscard]] MeshHandle GetMesh() const;
            [[nodiscard]] const std::vector<MaterialInstanceHandle>& GetMaterials() const;
            [[nodiscard]] Transform& GetTransform();
            [[nodiscard]] const Transform& GetTransform() const;

        private:
            MeshHandle Mesh;
            std::vector<MaterialInstanceHandle> Materials;
            Transform LocalTransform;
    };
}
