#include "Framework/Asset/Public/Asset.h"

#include <utility>

namespace ShadowEngine
{
    Asset::Asset(EAssetType InType, std::filesystem::path InPath)
        : Type(InType)
        , Path(std::move(InPath))
    {
    }

    EAssetType Asset::GetType() const
    {
        return Type;
    }

    const std::filesystem::path& Asset::GetPath() const
    {
        return Path;
    }

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
