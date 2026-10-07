#pragma once

#include "Framework/Asset/Public/Asset.h"
#include "Framework/Material/Public/Material.h"
#include "Framework/Material/Public/MaterialHandle.h"

#include <memory>

namespace ShadowEngine
{
    class MaterialAsset final : public Asset
    {
        public:
            MaterialAsset(
                std::filesystem::path InPath,
                std::unique_ptr<Material> InMaterial,
                MaterialHandle InHandle);

            [[nodiscard]] Material& GetMaterial();
            [[nodiscard]] const Material& GetMaterial() const;
            [[nodiscard]] MaterialHandle GetHandle() const;

        private:
            std::unique_ptr<Material> OwnedMaterial;
            MaterialHandle Handle;
    };
}
