#include "Framework/RHI/Public/RHIDevice.h"

namespace ShadowEngine
{
    std::unique_ptr<RHISampler> RHIDevice::CreatePointClampSampler(std::string* ErrorMessage)
    {
        return CreateClampSampler(ErrorMessage);
    }
}
