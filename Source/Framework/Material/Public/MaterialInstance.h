#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Material/Public/Material.h"
#include "Framework/Material/Public/MaterialParameter.h"

#include <filesystem>

namespace ShadowEngine
{
    class MaterialInstance final : public NonCopyable
    {
        public:
            MaterialInstance(
                const Material& InMaterial,
                std::filesystem::path InDescriptionPath,
                MaterialParameter InParameter);

            [[nodiscard]] const Material& GetMaterial() const;
            [[nodiscard]] const std::filesystem::path& GetDescriptionPath() const;
            [[nodiscard]] const MaterialParameter& GetParameter() const;

        private:
            const Material* Parent = nullptr;
            std::filesystem::path DescriptionPath;
            MaterialParameter Parameter;
    };
}
