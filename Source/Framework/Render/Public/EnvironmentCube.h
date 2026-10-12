#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Common/Public/Types.h"

#include <memory>
#include <string>

namespace ShadowEngine
{
    class RHIBuffer;
    class RHICommandList;
    class RHIDevice;
    class RHIPipeline;
    class RHITexture;
    class ShaderManager;

    class EnvironmentCube final : public NonCopyable
    {
        public:
            bool Ensure(
                RHIDevice& Device,
                ShaderManager& Shaders,
                RHICommandList& Commands,
                uint32 EquirectDescriptor,
                uint32 EquirectSampler,
                uint32 FaceSize,
                std::string* ErrorMessage);

            [[nodiscard]] bool IsReady() const
            {
                return bReady;
            }

            [[nodiscard]] uint32 GetDescriptor() const
            {
                return Descriptor;
            }

        private:
            struct GpuConstants
            {
                float DestSize[2];
                uint32 Face;
                uint32 Pad0;
                uint32 Source;
                uint32 Sampler;
                uint32 Pad1[2];
            };

            std::unique_ptr<RHITexture> Texture;
            std::unique_ptr<RHIPipeline> Pipeline;
            std::unique_ptr<RHIBuffer> ConstantBuffer;
            uint32 Descriptor = 0xffffffff;
            bool bReady = false;
    };
}
