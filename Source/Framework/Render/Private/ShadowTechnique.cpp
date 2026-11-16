#include "Framework/Render/Public/ShadowTechnique.h"

#include "Framework/Config/Public/ConfigManager.h"
#include "Framework/Render/Public/ForwardShadows.h"
#include "Framework/Render/Public/VirtualShadows.h"

namespace ShadowEngine
{
    std::unique_ptr<ShadowTechnique> CreateShadowTechnique()
    {
        if (ConfigManager::Get().GetRHISetting().Shadow == "VirtualShadow")
        {
            return std::make_unique<VirtualShadows>();
        }

        return std::make_unique<ForwardShadows>();
    }
}
