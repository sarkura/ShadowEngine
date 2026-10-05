#pragma once

#include "Framework/Asset/Public/Asset.h"
#include "Framework/Common/Public/NonCopyable.h"

#include <filesystem>
#include <memory>
#include <unordered_map>

namespace ShadowEngine
{
    class AssetRegistry final : public NonCopyable
    {
        public:
            bool Register(std::unique_ptr<Asset> InAsset);
            void Clear();

            [[nodiscard]] const Asset* Find(const std::filesystem::path& Path) const;
            [[nodiscard]] const MeshAsset* FindMesh(const std::filesystem::path& Path) const;

        private:
            std::unordered_map<std::string, std::unique_ptr<Asset>> Assets;
    };
}
