#pragma once

#include "Framework/Asset/Public/Asset.h"
#include "Framework/Material/Public/MaterialHandle.h"

#include <vector>

namespace ShadowEngine
{
    struct MeshVertex
    {
        float Position[3];
        float Normal[3];
        float Color[3];
        float TexCoord[2];
    };

    static_assert(sizeof(MeshVertex) == 44);

    struct MeshSection
    {
        std::vector<MeshVertex> Vertices;
        std::vector<uint32> Indices;
        MaterialInstanceHandle Material;
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
