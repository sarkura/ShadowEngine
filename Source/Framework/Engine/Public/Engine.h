#pragma once

#include "Framework/Asset/Public/AssetManager.h"
#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Common/Public/Types.h"
#include "Framework/Render/Public/Renderer.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/Scene/Public/Scene.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <memory>
#include <string>

namespace ShadowEngine
{
    struct EngineInitDesc
    {
        void* WindowHandle = nullptr;
        uint32 Width = 0;
        uint32 Height = 0;
    };

    class Engine final : public NonCopyable
    {
        public:
            Engine() = default;
            ~Engine();

            bool Initialize(const EngineInitDesc& Desc, std::string* ErrorMessage = nullptr);
            void Finalize();
            void Tick(float DeltaTime);
            void Resize(uint32 Width, uint32 Height);

            [[nodiscard]] bool IsInitialized() const;
            [[nodiscard]] bool IsRenderingPaused() const;

        private:
            bool InitializeRHI(const EngineInitDesc& Desc, std::string* ErrorMessage);

            std::unique_ptr<RHIDevice> Device;
            std::unique_ptr<RHISwapChain> SwapChain;
            std::unique_ptr<ShaderManager> Shaders;
            std::unique_ptr<AssetManager> Assets;
            std::unique_ptr<Scene> MainScene;
            std::unique_ptr<Renderer> MainRenderer;
            bool bInitialized = false;
            bool bRenderingPaused = false;
    };
}
