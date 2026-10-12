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
        const DirectLightRenderProxy* DirectLight = nullptr;
        const std::vector<PointLightRenderProxy>* PointLights = nullptr;
        const SkyLightRenderProxy* SkyLight = nullptr;
        RHIPipeline* SkyPipeline = nullptr;
        RHIPipeline* DepthPrepassPipeline = nullptr;
        RHIBuffer* SkyConstants = nullptr;
        const struct ImageBasedLightBinding* ImageBasedLight = nullptr;
        uint32 EnvironmentDescriptor = 0;
        uint32 EnvironmentSampler = 0;
        RHITexture* SceneColor = nullptr;
        class PostProcess* Post = nullptr;
        float ProjectionJitterX = 0.0F;
        float ProjectionJitterY = 0.0F;
        const float* PreviousViewProjection = nullptr;
        const std::vector<float>* PreviousWorlds = nullptr;
        float* StoredViewProjection = nullptr;
        RHIColor ClearColor;
        uint32 ConstantCapacity = 0;
        uint32 ConstantAlignment = 1024;
    };

    class RenderPass : public NonCopyable
    {
        public:
            RenderPass() = default;
            virtual ~RenderPass() = default;

            virtual bool Execute(const RenderQueue& Queue, const RenderPassContext& Context) = 0;
    };
}
