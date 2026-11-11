#include "Framework/UI/Public/RenderSettingsPanel.h"

#include "Framework/Config/Public/ConfigManager.h"
#include "Framework/Config/Public/UIConfigSetting.h"
#include "Framework/UI/Public/UIManager.h"

#include "ThirdPartyAdapter/UI/Public/ImGuiAdapter.h"

namespace ShadowEngine
{
    namespace
    {
        bool Slider(const char* Label, float* Value, float Minimum, float Maximum)
        {
            ImGuiAdapter::TextUnformatted(Label);
            ImGuiAdapter::SetNextItemWidth(-FLT_MIN);
            ImGuiAdapter::PushID(Label);
            const bool bChanged = ImGuiAdapter::SliderFloat("##Value", Value, Minimum, Maximum, "%.3f");
            ImGuiAdapter::PopID();
            return bChanged;
        }

        void PlacePanel(const char* Title)
        {
            const ImGuiViewport* Viewport = ImGuiAdapter::GetMainViewport();
            constexpr float PanelWidth = 320.0F;
            constexpr float Margin = 8.0F;
            const float PanelHeight = Viewport->WorkSize.y - Margin * 2.0F;
            ImGuiAdapter::SetNextWindowPos(
                ImVec2(Viewport->WorkPos.x + Viewport->WorkSize.x - PanelWidth - Margin, Viewport->WorkPos.y + Margin),
                ImGuiCond_Always);
            ImGuiAdapter::SetNextWindowSize(ImVec2(PanelWidth, PanelHeight > 1.0F ? PanelHeight : 1.0F), ImGuiCond_Always);
            constexpr ImGuiWindowFlags WindowFlags =
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
            ImGuiAdapter::Begin(Title, nullptr, WindowFlags);
        }

        template<typename PostSetting>
        bool EditPost(PostSetting& Post)
        {
            bool bChanged = false;
            if (ImGuiAdapter::CollapsingHeader("Post", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGuiAdapter::TreeNodeEx("TAA", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGuiAdapter::Checkbox("Enable", &Post.bTAA);
                    ImGuiAdapter::BeginDisabled(!Post.bTAA);
                    bChanged |= Slider("Current Weight", &Post.TAACurrentWeight, 0.01F, 1.0F);
                    ImGuiAdapter::EndDisabled();
                    ImGuiAdapter::TreePop();
                }

                if (ImGuiAdapter::TreeNodeEx("Auto Exposure", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGuiAdapter::Checkbox("Enable", &Post.bAutoExposure);
                    ImGuiAdapter::BeginDisabled(!Post.bAutoExposure);
                    bChanged |= Slider("Middle Grey", &Post.ExposureMiddleGrey, 0.01F, 1.0F);
                    bChanged |= Slider("Minimum", &Post.ExposureMinimum, 0.001F, 4.0F);
                    bChanged |= Slider("Maximum", &Post.ExposureMaximum, 0.05F, 32.0F);
                    bChanged |= Slider("Adapt Speed", &Post.ExposureAdaptSpeed, 0.0F, 10.0F);
                    ImGuiAdapter::EndDisabled();
                    ImGuiAdapter::TreePop();
                }

                if (ImGuiAdapter::TreeNodeEx("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGuiAdapter::Checkbox("Enable", &Post.bBloom);
                    ImGuiAdapter::BeginDisabled(!Post.bBloom);
                    bChanged |= Slider("Threshold", &Post.BloomThreshold, 0.0F, 8.0F);
                    bChanged |= Slider("Strength", &Post.BloomStrength, 0.0F, 1.0F);
                    bChanged |= Slider("Limit", &Post.BloomLimit, 0.5F, 16.0F);
                    ImGuiAdapter::EndDisabled();
                    ImGuiAdapter::TreePop();
                }

                if (ImGuiAdapter::TreeNodeEx("Tonemap", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGuiAdapter::Checkbox("Dither", &Post.bTonemapDither);
                    ImGuiAdapter::TreePop();
                }
            }
            return bChanged;
        }

        template<typename LightSetting>
        bool EditLight(LightSetting& Light)
        {
            bool bChanged = false;
            if (ImGuiAdapter::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGuiAdapter::TreeNodeEx("Shadows", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGuiAdapter::Checkbox("Directional", &Light.bDirectionalShadow);
                    ImGuiAdapter::BeginDisabled(!Light.bDirectionalShadow);
                    bChanged |= Slider("Normal Bias", &Light.ShadowNormalBias, 0.0F, 0.2F);
                    ImGuiAdapter::EndDisabled();
                    bChanged |= ImGuiAdapter::Checkbox("Point", &Light.bPointShadow);
                    ImGuiAdapter::BeginDisabled(!Light.bPointShadow);
                    bChanged |= Slider("Depth Bias", &Light.PointShadowDepthBias, 0.0F, 0.05F);
                    ImGuiAdapter::EndDisabled();
                    ImGuiAdapter::TreePop();
                }

                if (ImGuiAdapter::TreeNodeEx("Image Based Lighting", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGuiAdapter::Checkbox("Enable", &Light.bImageBasedLighting);
                    ImGuiAdapter::TreePop();
                }

                if (ImGuiAdapter::TreeNodeEx("Sky", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGuiAdapter::Checkbox("Enable", &Light.bSkyBackground);
                    ImGuiAdapter::TreePop();
                }
            }
            return bChanged;
        }

        template<typename InsightSetting>
        bool EditInsight(InsightSetting& Insight)
        {
            bool bChanged = false;
            if (ImGuiAdapter::CollapsingHeader("Insight", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGuiAdapter::TreeNodeEx("FPS", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGuiAdapter::Checkbox("Enable", &Insight.bShowFrameRate);
                    ImGuiAdapter::TreePop();
                }
            }
            return bChanged;
        }
    }

    void BuildRenderSettingsPanel()
    {
        const EUIPipeline Pipeline = UIManager::Get().GetPipeline();
        if (Pipeline == EUIPipeline::Deferred)
        {
            PlacePanel("Deferred");
            UIConfigSetting::DeferredUIParameter Setting = ConfigManager::Get().GetDeferredUISetting();
            const bool bChanged = EditInsight(Setting.Frame) | EditLight(Setting.Light) | EditPost(Setting.Post);
            if (bChanged)
            {
                ConfigManager::Get().SubmitDeferredUISetting(Setting);
            }
            ImGuiAdapter::End();
            return;
        }

        if (Pipeline == EUIPipeline::Forward)
        {
            PlacePanel("Forward");
            UIConfigSetting::ForwardUIParameter Setting = ConfigManager::Get().GetForwardUISetting();
            const bool bChanged = EditInsight(Setting.Frame) | EditLight(Setting.Light) | EditPost(Setting.Post);
            if (bChanged)
            {
                ConfigManager::Get().SubmitForwardUISetting(Setting);
            }
            ImGuiAdapter::End();
            return;
        }

        if (Pipeline == EUIPipeline::BlinnPhong)
        {
            PlacePanel("BlinnPhong");
            UIConfigSetting::BlinnPhongUIParameter Setting = ConfigManager::Get().GetBlinnPhongUISetting();
            if (EditInsight(Setting.Frame))
            {
                ConfigManager::Get().SubmitBlinnPhongUISetting(Setting);
            }
            ImGuiAdapter::End();
            return;
        }

        if (Pipeline == EUIPipeline::Debug)
        {
            PlacePanel("Debug");
            UIConfigSetting::DebugUIParameter Setting = ConfigManager::Get().GetDebugUISetting();
            if (EditInsight(Setting.Frame))
            {
                ConfigManager::Get().SubmitDebugUISetting(Setting);
            }
            ImGuiAdapter::End();
        }
    }

    void BuildFrameRateReadout()
    {
        if (!UIManager::Get().ShowsFrameRate())
        {
            return;
        }

        const ImGuiViewport* Viewport = ImGuiAdapter::GetMainViewport();
        ImGuiAdapter::SetNextWindowPos(ImVec2(Viewport->WorkPos.x + 12.0F, Viewport->WorkPos.y + 12.0F), ImGuiCond_Always);
        ImGuiAdapter::SetNextWindowBgAlpha(0.45F);
        constexpr ImGuiWindowFlags WindowFlags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoInputs;
        if (ImGuiAdapter::Begin("Frame Rate", nullptr, WindowFlags))
        {
            const int Rate = static_cast<int>(ImGuiAdapter::GetIO().Framerate + 0.5F);
            ImGuiAdapter::Text("%d", Rate);
        }
        ImGuiAdapter::End();
    }
}
