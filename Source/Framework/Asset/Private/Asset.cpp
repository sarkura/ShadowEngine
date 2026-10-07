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
}
