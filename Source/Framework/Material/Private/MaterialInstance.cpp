#include "Framework/Material/Public/MaterialInstance.h"

#include <utility>

namespace ShadowEngine
{
    MaterialInstance::MaterialInstance(MaterialHandle InBaseMaterial, MaterialParameterBlock InParameters)
        : BaseMaterial(InBaseMaterial)
        , Parameters(std::move(InParameters))
    {
    }

    MaterialHandle MaterialInstance::GetBaseMaterial() const
    {
        return BaseMaterial;
    }

    const MaterialParameterBlock& MaterialInstance::GetParameters() const
    {
        return Parameters;
    }
}
