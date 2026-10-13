#pragma once

#include "Framework/Asset/Public/Description.h"
#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Material/Public/MaterialHandle.h"
#include "Framework/Material/Public/MaterialParameterBlock.h"

#include <string>
#include <string_view>
#include <vector>

namespace ShadowEngine
{
    struct MaterialRendererParameters
    {
        std::string Renderer;
        EMaterialSurface Surface = EMaterialSurface::Opaque;
        bool bTwoSided = false;
        MaterialParameterBlock Parameters;
    };

    class MaterialInstance final : public NonCopyable
    {
        public:
            MaterialInstance(MaterialHandle InBaseMaterial, std::vector<MaterialRendererParameters> InParameters);

            [[nodiscard]] MaterialHandle GetBaseMaterial() const;
            [[nodiscard]] const MaterialParameterBlock* FindParameters(std::string_view RendererName) const;
            [[nodiscard]] EMaterialSurface FindSurface(std::string_view RendererName) const;
            [[nodiscard]] bool FindTwoSided(std::string_view RendererName) const;
            [[nodiscard]] bool HasTextures() const;

        private:
            MaterialHandle BaseMaterial;
            std::vector<MaterialRendererParameters> Parameters;
    };
}
