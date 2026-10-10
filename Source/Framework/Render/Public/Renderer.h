#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Render/Public/Camera.h"
#include "Framework/Render/Public/MeshBatch.h"
#include "Framework/Render/Public/RenderProxy.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "Framework/RHI/Public/RHISampler.h"
#include "Framework/RHI/Public/RHIShader.h"
#include "Framework/RHI/Public/RHITexture.h"
#include "Framework/RHI/Public/RHITypes.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ShadowEngine
{
    class AssetManager;
    class RHIDevice;
    class RHISwapChain;
    class Scene;
    class ShaderManager;

    class Renderer : public NonCopyable
    {
        public:
            Renderer() = default;
            virtual ~Renderer();

            bool Initialize(
                RHIDevice& InDevice,
                RHISwapChain& InSwapChain,
                ShaderManager& Shaders,
                AssetManager& InAssets,
                const Scene& InScene,
                const RHIColor& InClearColor,
                std::string* ErrorMessage = nullptr);
            void Finalize();

            void Sync(const Scene& InScene);
            virtual bool RenderFrame() = 0;
            bool Resize(uint32 Width, uint32 Height, std::string* ErrorMessage = nullptr);
            void UpdateCamera(float DeltaTime, float Forward, float Right, float Up, float Yaw, float Pitch);
            void SetCameraMoveSpeed(float ForwardSpeed, float RightSpeed, float UpSpeed, float DownSpeed);

        protected:
            [[nodiscard]] virtual const char* GetName() const = 0;

            static constexpr uint32 ConstantAlignment = 512;

            RHIDevice* Device = nullptr;
            RHISwapChain* SwapChain = nullptr;
            AssetManager* Assets = nullptr;
            ShaderManager* ShaderLibrary = nullptr;
            RHIColor ClearColor;

            std::unique_ptr<RHICommandList> CommandList;
            std::unique_ptr<RHIBuffer> ConstantBuffer;
            std::unique_ptr<RHIPipeline> SkyPipeline;
            std::unique_ptr<RHIBuffer> SkyConstantBuffer;
            uint32 EnvironmentDescriptor = 0;
            uint32 EnvironmentSampler = 0;
            std::unique_ptr<RHITexture> DepthBuffer;
            std::unordered_map<uint32, std::unique_ptr<RHISampler>> GpuSamplers;
            std::unordered_map<uint32, uint32> TextureDescriptors;
            std::vector<MeshBatch> MeshBatches;
            RenderProxy SceneProxy;
            uint32 ConstantCapacity = 0;
            Camera ViewCamera;

        private:
            bool UploadMesh(ShaderManager& Shaders, MeshHandle Mesh, std::string* ErrorMessage);
            bool UploadEnvironment(ShaderManager& Shaders, std::string* ErrorMessage);
            RHITexture* UploadTexture(TextureAssetHandle Texture, std::string* ErrorMessage);
            RHISampler* UploadSampler(SamplerHandle Sampler, std::string* ErrorMessage);

            std::unordered_map<uint32, std::unique_ptr<RHITexture>> GpuTextures;
    };
}
