#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Common/Public/Types.h"

#include <filesystem>

namespace ShadowEngine
{
    enum class EAssetType : uint8
    {
        Mesh,
        Texture,
        Material,
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
}
