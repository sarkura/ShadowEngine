#include "Framework/Asset/Public/AssetRegistry.h"

#include "Framework/Asset/Public/MaterialAsset.h"
#include "Framework/Asset/Public/MeshAsset.h"
#include "Framework/Asset/Public/TextureAsset.h"

namespace ShadowEngine
{
    bool AssetRegistry::Register(std::unique_ptr<Asset> InAsset)
    {
        if (InAsset == nullptr)
        {
            return false;
        }

        const std::string Key = InAsset->GetPath().generic_string();
        if (Assets.find(Key) != Assets.end())
        {
            return false;
        }

        Assets.emplace(Key, std::move(InAsset));
        return true;
    }

    void AssetRegistry::Clear()
    {
        Assets.clear();
    }

    const Asset* AssetRegistry::Find(const std::filesystem::path& Path) const
    {
        const auto Found = Assets.find(Path.generic_string());
        if (Found == Assets.end())
        {
            return nullptr;
        }

        return Found->second.get();
    }

    const MeshAsset* AssetRegistry::FindMesh(const std::filesystem::path& Path) const
    {
        const Asset* Found = Find(Path);
        if (Found == nullptr || Found->GetType() != EAssetType::Mesh)
        {
            return nullptr;
        }

        return static_cast<const MeshAsset*>(Found);
    }

    const TextureAsset* AssetRegistry::FindTexture(const std::filesystem::path& Path) const
    {
        const Asset* Found = Find(Path);
        if (Found == nullptr || Found->GetType() != EAssetType::Texture)
        {
            return nullptr;
        }

        return static_cast<const TextureAsset*>(Found);
    }

    const MaterialAsset* AssetRegistry::FindMaterial(const std::filesystem::path& Path) const
    {
        const Asset* Found = Find(Path);
        if (Found == nullptr || Found->GetType() != EAssetType::Material)
        {
            return nullptr;
        }

        return static_cast<const MaterialAsset*>(Found);
    }
}
