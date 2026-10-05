#include "Framework/Material/Public/MaterialInstance.h"

#include <utility>

namespace ShadowEngine
{
    MaterialInstance::MaterialInstance(
        const Material& InMaterial,
        std::filesystem::path InDescriptionPath,
        MaterialParameter InParameter)
        : Parent(&InMaterial)
        , DescriptionPath(std::move(InDescriptionPath))
        , Parameter(std::move(InParameter))
    {
    }

    const Material& MaterialInstance::GetMaterial() const
    {
        return *Parent;
    }

    const std::filesystem::path& MaterialInstance::GetDescriptionPath() const
    {
        return DescriptionPath;
    }

    const MaterialParameter& MaterialInstance::GetParameter() const
    {
        return Parameter;
    }
}
