#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Render/Public/Camera.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "Framework/RHI/Public/RHIShader.h"
#include "Framework/RHI/Public/RHITexture.h"
#include "Framework/RHI/Public/RHITypes.h"
#include "Framework/Scene/Public/Scene.h"

#include <memory>
#include <string>
#include <vector>

namespace ShadowEngine
{
    class MeshAsset;
    class RHIDevice;
    class RHISwapChain;
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
                const Scene& InScene,
                const RHIColor& InClearColor,
                std::string* ErrorMessage = nullptr);
            void Finalize();

            bool RenderFrame(const Scene& InScene);
            bool Resize(uint32 Width, uint32 Height, std::string* ErrorMessage = nullptr);
            void UpdateCamera(float DeltaTime, float Forward, float Right, float Yaw, float Pitch);
            void SetCameraMoveSpeed(float ForwardSpeed, float RightSpeed);

        private:
            struct MeshBatch
            {
                const MeshAsset* Asset = nullptr;
                std::unique_ptr<RHIShader> VertexShader;
                std::unique_ptr<RHIShader> PixelShader;
                std::unique_ptr<RHIBuffer> VertexBuffer;
                std::unique_ptr<RHIBuffer> IndexBuffer;
                std::unique_ptr<RHIPipeline> Pipeline;
                uint32 IndexCount = 0;
            };

            bool UploadMesh(ShaderManager& Shaders, const MeshAsset& Mesh, std::string* ErrorMessage);

            RHIDevice* Device = nullptr;
            RHISwapChain* SwapChain = nullptr;
            RHIColor ClearColor;

            std::unique_ptr<RHICommandList> CommandList;
            std::unique_ptr<RHIBuffer> ConstantBuffer;
            std::unique_ptr<RHITexture> DepthBuffer;
            std::vector<MeshBatch> MeshBatches;
            uint32 ConstantCapacity = 0;
            Camera ViewCamera;
    };
}
