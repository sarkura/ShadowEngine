#pragma once

#include "Framework/Asset/Public/AssetHandle.h"
#include "Framework/Asset/Public/AssetRegistry.h"
#include "Framework/Asset/Public/MeshAsset.h"
#include "Framework/Asset/Public/TextureAsset.h"
#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Material/Public/Material.h"
#include "Framework/Material/Public/MaterialHandle.h"
#include "Framework/Material/Public/MaterialInstance.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ShadowEngine
{
    class ShaderManager;

    class AssetManager final : public NonCopyable
    {
        public:
            AssetManager() = default;
            ~AssetManager();

            void Finalize();
            void SetShaderManager(ShaderManager& InShaders);

            MeshHandle LoadMesh(
                const std::filesystem::path& Path,
                std::string* ErrorMessage = nullptr);

            [[nodiscard]] const AssetRegistry& GetRegistry() const;
            [[nodiscard]] const MeshAsset* ResolveMesh(MeshHandle Handle) const;
            [[nodiscard]] const TextureAsset* ResolveTexture(TextureAssetHandle Handle) const;
            [[nodiscard]] const Material* ResolveMaterial(MaterialHandle Handle) const;
            [[nodiscard]] const MaterialInstance* ResolveMaterialInstance(MaterialInstanceHandle Handle) const;

        private:
            MaterialInstanceHandle LoadMaterialInstance(
                const std::filesystem::path& DescriptionPath,
                std::string* ErrorMessage);

            TextureAssetHandle LoadTexture(
                const std::filesystem::path& Path,
                std::string* ErrorMessage);

            [[nodiscard]] SamplerHandle DefaultSampler();

            ShaderManager* Shaders = nullptr;
            AssetRegistry Registry;
            std::vector<const MeshAsset*> Meshes;
            std::vector<const TextureAsset*> Textures;
            std::vector<Material*> Materials;
            std::vector<std::unique_ptr<MaterialInstance>> MaterialInstances;
            SamplerHandle LinearSampler;
    };
}
