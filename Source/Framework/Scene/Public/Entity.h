#pragma once

#include "Framework/Scene/Public/Transform.h"

namespace ShadowEngine
{
    class MeshAsset;

    class Entity final
    {
        public:
            void SetMesh(const MeshAsset* InMesh);

            [[nodiscard]] const MeshAsset* GetMesh() const;
            [[nodiscard]] Transform& GetTransform();
            [[nodiscard]] const Transform& GetTransform() const;

        private:
            const MeshAsset* Mesh = nullptr;
            Transform LocalTransform;
    };
}
