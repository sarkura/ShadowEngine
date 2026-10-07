#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Render/Public/Camera.h"
#include "Framework/Render/Public/RenderProxy.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIDescriptor.h"
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

    class Renderer final : public NonCopyable
    {
        public:
            Renderer() = default;
            ~Renderer();

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
            bool RenderFrame();
            bool Resize(uint32 Width, uint32 Height, std::string* ErrorMessage = nullptr);
            void UpdateCamera(float DeltaTime, float Forward, float Right, float Up, float Yaw, float Pitch);
            void SetCameraMoveSpeed(float ForwardSpeed, float RightSpeed, float UpSpeed, float DownSpeed);

        private:
            struct MeshBatch
            {
                MeshHandle Mesh;
                RHIShader* VertexShader = nullptr;
                RHIShader* PixelShader = nullptr;
                std::unique_ptr<RHIBuffer> VertexBuffer;
                std::unique_ptr<RHIBuffer> IndexBuffer;
                std::unique_ptr<RHIPipeline> Pipeline;
                std::unique_ptr<RHIMaterialBinding> MaterialBinding;
                uint32 IndexCount = 0;
            };

            bool UploadMesh(ShaderManager& Shaders, MeshHandle Mesh, std::string* ErrorMessage);
            RHITexture* UploadTexture(TextureAssetHandle Texture, std::string* ErrorMessage);
            RHISampler* UploadSampler(SamplerHandle Sampler, std::string* ErrorMessage);

            RHIDevice* Device = nullptr;
            RHISwapChain* SwapChain = nullptr;
            AssetManager* Assets = nullptr;
            RHIColor ClearColor;

            std::unique_ptr<RHICommandList> CommandList;
            std::unique_ptr<RHIBuffer> ConstantBuffer;
            std::unique_ptr<RHITexture> DepthBuffer;
            std::unordered_map<uint32, std::unique_ptr<RHISampler>> GpuSamplers;
            std::unordered_map<uint32, std::unique_ptr<RHITexture>> GpuTextures;
            std::vector<MeshBatch> MeshBatches;
            RenderProxy SceneProxy;
            uint32 ConstantCapacity = 0;
            Camera ViewCamera;
    };
}
