#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Render/Public/Camera.h"
#include "Framework/Render/Public/MeshBatch.h"
#include "Framework/Render/Public/RenderQueue.h"
#include "Framework/RHI/Public/RHISampler.h"
#include "Framework/RHI/Public/RHITypes.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace ShadowEngine
{
    class AssetManager;
    class RHIBuffer;
    class RHICommandList;
    class RHIDevice;
    class RHIPipeline;
    class RHISwapChain;
    class RHITexture;
    class ShaderManager;

    struct RenderPassContext
    {
        RHIDevice* Device = nullptr;
        RHISwapChain* SwapChain = nullptr;
        RHICommandList* CommandList = nullptr;
        RHIBuffer* ConstantBuffer = nullptr;
        RHITexture* DepthBuffer = nullptr;
        AssetManager* Assets = nullptr;
        ShaderManager* Shaders = nullptr;
        const Camera* View = nullptr;
        const std::vector<MeshBatch>* Batches = nullptr;
        const std::unordered_map<uint32, uint32>* TextureDescriptors = nullptr;
        const std::unordered_map<uint32, std::unique_ptr<RHISampler>>* Samplers = nullptr;
        const std::vector<DirectLightRenderProxy>* Lights = nullptr;
        const std::vector<PointLightRenderProxy>* PointLights = nullptr;
        const std::vector<SkyLightRenderProxy>* SkyLights = nullptr;
        RHIPipeline* SkyPipeline = nullptr;
        RHIBuffer* SkyConstants = nullptr;
        uint32 EnvironmentDescriptor = 0;
        uint32 EnvironmentSampler = 0;
        RHIColor ClearColor;
        uint32 ConstantCapacity = 0;
        uint32 ConstantAlignment = 512;
    };

    class RenderPass : public NonCopyable
    {
        public:
            RenderPass() = default;
            virtual ~RenderPass() = default;

            virtual bool Execute(const RenderQueue& Queue, const RenderPassContext& Context) = 0;
    };
}
