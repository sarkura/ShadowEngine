#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Common/Public/Types.h"

#include <filesystem>
#include <vector>

namespace ShadowEngine
{
    class MaterialInstance;

    enum class EAssetType : uint8
    {
        Mesh,
    };

    struct MeshVertex
    {
        float Position[3];
        float Normal[3];
        float Color[3];
    };

    static_assert(sizeof(MeshVertex) == 36);

    struct MeshSection
    {
        std::vector<MeshVertex> Vertices;
        std::vector<uint32> Indices;
        const MaterialInstance* Material = nullptr;
    };

    class Asset : public NonCopyable
    {
        public:
            virtual ~Asset() = default;

            [[nodiscard]] EAssetType GetType() const;
            [[nodiscard]] const std::filesystem::path& GetPath() const;

        protected:
            Asset(EAssetType InType, std::filesystem::path InPath);

        private:
            EAssetType Type;
            std::filesystem::path Path;
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
