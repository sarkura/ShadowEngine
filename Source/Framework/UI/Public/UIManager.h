#pragma once

#include "Framework/Common/Public/Types.h"
#include "Framework/Config/Public/UIConfigSetting.h"

namespace ShadowEngine
{
    enum class EUIPipeline : uint8
    {
        None,
        Deferred,
        Forward,
        BlinnPhong,
        Debug
    };

    class UIManager final
    {
        public:
            static UIManager& Get();
            ~UIManager();

            bool Initialize(EUIPipeline Pipeline);
            void Shutdown();

            [[nodiscard]] bool IsActive() const;
            [[nodiscard]] bool IsVisible() const;
            [[nodiscard]] bool ShowsFrameRate() const;
            [[nodiscard]] EUIPipeline GetPipeline() const;

            void ToggleVisible();
            void BeginFrame();

            struct PostState
            {
                bool bTAA = true;
                float TAACurrentWeight = 0.1F;
                bool bAutoExposure = true;
                float ExposureMiddleGrey = 0.18F;
                float ExposureMinimum = 0.05F;
                float ExposureMaximum = 8.0F;
                float ExposureAdaptSpeed = 2.0F;
                bool bBloom = true;
                float BloomThreshold = 1.0F;
                float BloomStrength = 0.08F;
                float BloomLimit = 4.0F;
                bool bTonemapDither = true;
            };

            struct LightState
            {
                bool bDirectionalShadow = true;
                bool bPointShadow = true;
                float ShadowNormalBias = 0.01F;
                float PointShadowDepthBias = 0.004F;
                bool bImageBasedLighting = true;
                bool bSkyBackground = true;
            };

            struct State
            {
                PostState Post;
                LightState Light;
            };

            [[nodiscard]] State GetState() const;

            UIManager(const UIManager&) = delete;
            UIManager& operator=(const UIManager&) = delete;
            UIManager(UIManager&&) = delete;
            UIManager& operator=(UIManager&&) = delete;

        private:
            UIManager() = default;

            EUIPipeline Pipeline = EUIPipeline::None;
            bool bActive = false;
            bool bVisible = true;
    };
}
