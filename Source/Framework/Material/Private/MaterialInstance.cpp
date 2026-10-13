#include "Framework/Material/Public/MaterialInstance.h"

#include "Framework/Material/Public/MaterialParameter.h"

#include <utility>

namespace ShadowEngine
{
    MaterialInstance::MaterialInstance(MaterialHandle InBaseMaterial, std::vector<MaterialRendererParameters> InParameters)
        : BaseMaterial(InBaseMaterial)
        , Parameters(std::move(InParameters))
    {
    }

    MaterialHandle MaterialInstance::GetBaseMaterial() const
    {
        return BaseMaterial;
    }

    const MaterialParameterBlock* MaterialInstance::FindParameters(std::string_view RendererName) const
    {
        for (const MaterialRendererParameters& Item : Parameters)
        {
            if (Item.Renderer == RendererName)
            {
                return &Item.Parameters;
            }
        }

        return nullptr;
    }

    EMaterialSurface MaterialInstance::FindSurface(std::string_view RendererName) const
    {
        for (const MaterialRendererParameters& Item : Parameters)
        {
            if (Item.Renderer == RendererName)
            {
                return Item.Surface;
            }
        }

        return EMaterialSurface::Opaque;
    }

    bool MaterialInstance::FindTwoSided(std::string_view RendererName) const
    {
        for (const MaterialRendererParameters& Item : Parameters)
        {
            if (Item.Renderer == RendererName)
            {
                return Item.bTwoSided;
            }
        }

        return false;
    }

    bool MaterialInstance::HasTextures() const
    {
        for (const MaterialRendererParameters& Item : Parameters)
        {
            if (MaterialParameter::GetTextureBinding(Item.Parameters).HasTextures())
            {
                return true;
            }
        }

        return false;
    }
}
