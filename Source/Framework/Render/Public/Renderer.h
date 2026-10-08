#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Render/Public/Camera.h"
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
            struct MeshBatch
            {
                MeshHandle Mesh;
                uint32 Section = 0;
                RHIShader* VertexShader = nullptr;
                RHIShader* PixelShader = nullptr;
                std::unique_ptr<RHIBuffer> VertexBuffer;
                std::unique_ptr<RHIBuffer> IndexBuffer;
                std::unique_ptr<RHIPipeline> Pipeline;
                uint32 IndexCount = 0;
            };

            struct FrameDraw
            {
                const MeshBatch* Batch = nullptr;
                const MeshRenderProxy* Proxy = nullptr;
                MaterialInstanceHandle Material;
            };

            [[nodiscard]] virtual const char* GetName() const = 0;
            bool CollectDraws(std::vector<FrameDraw>& Draws) const;
            bool SubmitDraws(const std::vector<FrameDraw>& Draws);

            static constexpr uint32 ConstantAlignment = 512;

            RHIDevice* Device = nullptr;
            RHISwapChain* SwapChain = nullptr;
            AssetManager* Assets = nullptr;
            RHIColor ClearColor;

            std::unique_ptr<RHICommandList> CommandList;
            std::unique_ptr<RHIBuffer> ConstantBuffer;
            std::unique_ptr<RHITexture> DepthBuffer;
            std::unordered_map<uint32, std::unique_ptr<RHISampler>> GpuSamplers;
            std::unordered_map<uint32, uint32> TextureDescriptors;
            std::vector<MeshBatch> MeshBatches;
            RenderProxy SceneProxy;
            uint32 ConstantCapacity = 0;
            Camera ViewCamera;

        private:
            bool UploadMesh(ShaderManager& Shaders, MeshHandle Mesh, std::string* ErrorMessage);
            RHITexture* UploadTexture(TextureAssetHandle Texture, std::string* ErrorMessage);
            RHISampler* UploadSampler(SamplerHandle Sampler, std::string* ErrorMessage);

            std::unordered_map<uint32, std::unique_ptr<RHITexture>> GpuTextures;
    };
}
