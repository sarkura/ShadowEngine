#pragma once

#include "Framework/Asset/Public/AssetRegistry.h"
#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Material/Public/Material.h"
#include "Framework/Material/Public/MaterialInstance.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ShadowEngine
{
    class AssetManager final : public NonCopyable
    {
        public:
            AssetManager() = default;
            ~AssetManager();

            void Finalize();

            const MeshAsset* LoadMesh(
                const std::filesystem::path& Path,
                std::string* ErrorMessage = nullptr);

            [[nodiscard]] const AssetRegistry& GetRegistry() const;

        private:
            const MaterialInstance* LoadMaterialInstance(
                const std::filesystem::path& DescriptionPath,
                std::string* ErrorMessage);

            AssetRegistry Registry;
            std::vector<std::unique_ptr<Material>> Materials;
            std::vector<std::unique_ptr<MaterialInstance>> MaterialInstances;
    };
}
