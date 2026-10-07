#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Material/Public/MaterialHandle.h"
#include "Framework/Material/Public/MaterialParameterBlock.h"

namespace ShadowEngine
{
    class MaterialInstance final : public NonCopyable
    {
        public:
            MaterialInstance(MaterialHandle InBaseMaterial, MaterialParameterBlock InParameters);

            [[nodiscard]] MaterialHandle GetBaseMaterial() const;
            [[nodiscard]] const MaterialParameterBlock& GetParameters() const;

        private:
            MaterialHandle BaseMaterial;
            MaterialParameterBlock Parameters;
    };
}
