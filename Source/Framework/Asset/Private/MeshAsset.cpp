#include "Framework/Asset/Public/MeshAsset.h"

#include <utility>

namespace ShadowEngine
{
    MeshAsset::MeshAsset(std::filesystem::path InPath, std::vector<MeshSection> InSections)
        : Asset(EAssetType::Mesh, std::move(InPath))
        , Sections(std::move(InSections))
    {
    }

    const std::vector<MeshSection>& MeshAsset::GetSections() const
    {
        return Sections;
    }
}
