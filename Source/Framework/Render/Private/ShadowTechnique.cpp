#include "Framework/Render/Public/ShadowTechnique.h"

#include "Framework/Render/Public/VirtualShadows.h"

namespace ShadowEngine
{
    std::unique_ptr<ShadowTechnique> CreateShadowTechnique()
    {
        return std::make_unique<VirtualShadows>();
    }
}
