#include "Framework/Asset/Public/MaterialAsset.h"

#include <utility>

namespace ShadowEngine
{
    MaterialAsset::MaterialAsset(
        std::filesystem::path InPath,
        std::unique_ptr<Material> InMaterial,
        MaterialHandle InHandle)
        : Asset(EAssetType::Material, std::move(InPath))
        , OwnedMaterial(std::move(InMaterial))
        , Handle(InHandle)
    {
    }

    Material& MaterialAsset::GetMaterial()
    {
        return *OwnedMaterial;
    }

    const Material& MaterialAsset::GetMaterial() const
    {
        return *OwnedMaterial;
    }

    MaterialHandle MaterialAsset::GetHandle() const
    {
        return Handle;
    }
}
