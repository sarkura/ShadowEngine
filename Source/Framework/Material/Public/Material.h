#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Material/Public/MaterialParameter.h"

#include <filesystem>
#include <string>

namespace ShadowEngine
{
    struct MaterialDescription
    {
        std::filesystem::path ShaderPath;
        MaterialParameter Parameter;
    };

    [[nodiscard]] bool LoadMaterialDescription(
        const std::filesystem::path& FilePath,
        MaterialDescription& Description,
        std::string* ErrorMessage = nullptr);

    class Material final : public NonCopyable
    {
        public:
            explicit Material(std::filesystem::path InShaderPath);

            [[nodiscard]] const std::filesystem::path& GetShaderPath() const;

        private:
            std::filesystem::path ShaderPath;
    };
}
