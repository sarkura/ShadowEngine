#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Common/Public/Types.h"

#include <memory>
#include <string>
#include <vector>

namespace ShadowEngine
{
    class RHIBuffer;
    class RHICommandList;
    class RHIDevice;
    class RHIPipeline;
    class RHISampler;
    class RHITexture;
    class ShaderManager;

    class UIRender final : public NonCopyable
    {
        public:
            bool Ensure(RHIDevice& Device, ShaderManager& Shaders, std::string* ErrorMessage = nullptr);
            void Draw(RHICommandList& Commands, RHITexture& Target);

        private:
            struct FontTexture
            {
                void* Source = nullptr;
                std::unique_ptr<RHITexture> Texture;
                uint32 Descriptor = 0xffffffff;
            };

            bool CreatePipeline(ShaderManager& Shaders, std::string* ErrorMessage);
            [[nodiscard]] int FindTexture(const void* Source) const;
            void UpdateTextures();
            bool UploadGeometry(const void* Vertices, uint32 VertexCount, const void* Indices, uint32 IndexCount);

            RHIDevice* Device = nullptr;
            std::unique_ptr<RHIPipeline> Pipeline;
            std::unique_ptr<RHIBuffer> VertexBuffer;
            std::unique_ptr<RHIBuffer> IndexBuffer;
            std::unique_ptr<RHIBuffer> ConstantBuffer;
            std::unique_ptr<RHISampler> LinearSampler;
            std::unique_ptr<RHISampler> PointSampler;
            std::vector<FontTexture> Textures;
            uint32 VertexCapacity = 0;
            uint32 IndexCapacity = 0;
    };
}
