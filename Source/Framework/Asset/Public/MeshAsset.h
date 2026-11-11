#pragma once

#include "CookedAsset/Public/MeshVertex.h"
#include "Framework/Asset/Public/Asset.h"
#include "Framework/Material/Public/MaterialHandle.h"

#include <vector>

namespace ShadowEngine
{
    struct MeshSection
    {
        std::vector<MeshVertex> Vertices;
        std::vector<uint32> Indices;
        MaterialInstanceHandle Material;
        float BoundsMin[3] = {};
        float BoundsMax[3] = {};
    };

    class MeshAsset final : public Asset
    {
        public:
            MeshAsset(std::filesystem::path InPath, std::vector<MeshSection> InSections);

            [[nodiscard]] const std::vector<MeshSection>& GetSections() const;

        private:
            std::vector<MeshSection> Sections;
    };
}
