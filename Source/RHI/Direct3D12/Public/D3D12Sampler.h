#pragma once

#include "Framework/RHI/Public/RHISampler.h"

namespace ShadowEngine
{
    class D3D12Sampler final : public RHISampler
    {
        public:
            explicit D3D12Sampler(uint32 InIndex);

            [[nodiscard]] uint32 GetDescriptorIndex() const override;

        private:
            uint32 Index = 0;
    };
}
