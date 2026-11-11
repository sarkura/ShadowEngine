#include "Framework/UI/Public/UIManager.h"

#include "Framework/Config/Public/ConfigManager.h"
#include "Framework/UI/Public/RenderSettingsPanel.h"

#include "ThirdPartyAdapter/UI/Public/ImGuiAdapter.h"

namespace ShadowEngine
{
    namespace
    {
        void SamplerLinearCallback(const ImDrawList*, const ImDrawCmd*)
        {
        }

        void SamplerNearestCallback(const ImDrawList*, const ImDrawCmd*)
        {
        }

        void ResetStateCallback(const ImDrawList*, const ImDrawCmd*)
        {
        }

        template<typename PostSetting>
        UIManager::PostState CopyPost(const PostSetting& Setting)
        {
            UIManager::PostState State;
            State.bTAA = Setting.bTAA;
            State.TAACurrentWeight = Setting.TAACurrentWeight;
            State.bAutoExposure = Setting.bAutoExposure;
            State.ExposureMiddleGrey = Setting.ExposureMiddleGrey;
            State.ExposureMinimum = Setting.ExposureMinimum;
            State.ExposureMaximum = Setting.ExposureMaximum;
            State.ExposureAdaptSpeed = Setting.ExposureAdaptSpeed;
            State.bBloom = Setting.bBloom;
            State.BloomThreshold = Setting.BloomThreshold;
            State.BloomStrength = Setting.BloomStrength;
            State.BloomLimit = Setting.BloomLimit;
            State.bTonemapDither = Setting.bTonemapDither;
            return State;
        }

        template<typename LightSetting>
        UIManager::LightState CopyLight(const LightSetting& Setting)
        {
            UIManager::LightState State;
            State.bDirectionalShadow = Setting.bDirectionalShadow;
            State.bPointShadow = Setting.bPointShadow;
            State.ShadowNormalBias = Setting.ShadowNormalBias;
            State.PointShadowDepthBias = Setting.PointShadowDepthBias;
            State.bImageBasedLighting = Setting.bImageBasedLighting;
            State.bSkyBackground = Setting.bSkyBackground;
            return State;
        }
    }

    UIManager& UIManager::Get()
    {
        static UIManager Instance;
        return Instance;
    }

    UIManager::~UIManager()
    {
        Shutdown();
    }

    bool UIManager::Initialize(EUIPipeline InPipeline)
    {
        if (InPipeline == EUIPipeline::None)
        {
            return true;
        }
        if (bActive)
        {
            return true;
        }

        if (!ImGuiAdapter::HasContext())
        {
            ImGuiAdapter::CreateContext();
        }

        ImGuiIO& IO = ImGuiAdapter::GetIO();
        IO.IniFilename = nullptr;
        IO.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        IO.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
        IO.BackendRendererName = "ShadowEngine";

        ImGuiPlatformIO& Platform = ImGuiAdapter::GetPlatformIO();
        Platform.DrawCallback_SetSamplerLinear = &SamplerLinearCallback;
        Platform.DrawCallback_SetSamplerNearest = &SamplerNearestCallback;
        Platform.DrawCallback_ResetRenderState = &ResetStateCallback;

        Pipeline = InPipeline;
        bActive = true;
        bVisible = true;
        return true;
    }

    void UIManager::Shutdown()
    {
        if (ImGuiAdapter::HasContext())
        {
            ImGuiAdapter::DestroyContext();
        }
        Pipeline = EUIPipeline::None;
        bActive = false;
        bVisible = true;
    }

    bool UIManager::IsActive() const
    {
        return bActive;
    }

    bool UIManager::IsVisible() const
    {
        return bActive && bVisible;
    }

    void UIManager::ToggleVisible()
    {
        if (!bActive)
        {
            return;
        }

        bVisible = !bVisible;
    }

    EUIPipeline UIManager::GetPipeline() const
    {
        return Pipeline;
    }

    bool UIManager::ShowsFrameRate() const
    {
        if (!bActive)
        {
            return false;
        }

        switch (Pipeline)
        {
            case EUIPipeline::Deferred:
                return ConfigManager::Get().GetDeferredUISetting().Frame.bShowFrameRate;
            case EUIPipeline::Forward:
                return ConfigManager::Get().GetForwardUISetting().Frame.bShowFrameRate;
            case EUIPipeline::BlinnPhong:
                return ConfigManager::Get().GetBlinnPhongUISetting().Frame.bShowFrameRate;
            case EUIPipeline::Debug:
                return ConfigManager::Get().GetDebugUISetting().Frame.bShowFrameRate;
            case EUIPipeline::None:
                return false;
        }

        return false;
    }

    void UIManager::BeginFrame()
    {
        if (!bActive)
        {
            return;
        }

        ImGuiAdapter::Win32NewFrame();
        ImGuiAdapter::NewFrame();
        if (bVisible)
        {
            BuildRenderSettingsPanel();
        }
        BuildFrameRateReadout();
        ImGuiAdapter::Render();
    }

    UIManager::State UIManager::GetState() const
    {
        State Resolved;
        if (Pipeline == EUIPipeline::Deferred)
        {
            const UIConfigSetting::DeferredUIParameter Setting = ConfigManager::Get().GetDeferredUISetting();
            Resolved.Post = CopyPost(Setting.Post);
            Resolved.Light = CopyLight(Setting.Light);
        }
        else if (Pipeline == EUIPipeline::Forward)
        {
            const UIConfigSetting::ForwardUIParameter Setting = ConfigManager::Get().GetForwardUISetting();
            Resolved.Post = CopyPost(Setting.Post);
            Resolved.Light = CopyLight(Setting.Light);
        }
        return Resolved;
    }
}
