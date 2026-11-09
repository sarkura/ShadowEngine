#include "Framework/UI/Public/RenderSettingsPanel.h"

#include "Framework/Config/Public/ConfigManager.h"
#include "Framework/Config/Public/UIConfigSetting.h"
#include "Framework/UI/Public/UIManager.h"

#include <imgui.h>

namespace ShadowEngine
{
    namespace
    {
        bool Slider(const char* Label, float* Value, float Minimum, float Maximum)
        {
            ImGui::TextUnformatted(Label);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::PushID(Label);
            const bool bChanged = ImGui::SliderFloat("##Value", Value, Minimum, Maximum, "%.3f");
            ImGui::PopID();
            return bChanged;
        }

        void PlacePanel(const char* Title)
        {
            const ImGuiViewport* Viewport = ImGui::GetMainViewport();
            constexpr float PanelWidth = 320.0F;
            constexpr float Margin = 8.0F;
            const float PanelHeight = Viewport->WorkSize.y - Margin * 2.0F;
            ImGui::SetNextWindowPos(
                ImVec2(Viewport->WorkPos.x + Viewport->WorkSize.x - PanelWidth - Margin, Viewport->WorkPos.y + Margin),
                ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(PanelWidth, PanelHeight > 1.0F ? PanelHeight : 1.0F), ImGuiCond_Always);
            constexpr ImGuiWindowFlags WindowFlags =
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
            ImGui::Begin(Title, nullptr, WindowFlags);
        }

        template<typename PostSetting>
        bool EditPost(PostSetting& Post)
        {
            bool bChanged = false;
            if (ImGui::CollapsingHeader("Post", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::TreeNodeEx("TAA", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGui::Checkbox("Enable", &Post.bTAA);
                    ImGui::BeginDisabled(!Post.bTAA);
                    bChanged |= Slider("Current Weight", &Post.TAACurrentWeight, 0.01F, 1.0F);
                    ImGui::EndDisabled();
                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Auto Exposure", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGui::Checkbox("Enable", &Post.bAutoExposure);
                    ImGui::BeginDisabled(!Post.bAutoExposure);
                    bChanged |= Slider("Middle Grey", &Post.ExposureMiddleGrey, 0.01F, 1.0F);
                    bChanged |= Slider("Minimum", &Post.ExposureMinimum, 0.001F, 4.0F);
                    bChanged |= Slider("Maximum", &Post.ExposureMaximum, 0.05F, 32.0F);
                    bChanged |= Slider("Adapt Speed", &Post.ExposureAdaptSpeed, 0.0F, 10.0F);
                    ImGui::EndDisabled();
                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGui::Checkbox("Enable", &Post.bBloom);
                    ImGui::BeginDisabled(!Post.bBloom);
                    bChanged |= Slider("Threshold", &Post.BloomThreshold, 0.0F, 8.0F);
                    bChanged |= Slider("Strength", &Post.BloomStrength, 0.0F, 1.0F);
                    bChanged |= Slider("Limit", &Post.BloomLimit, 0.5F, 16.0F);
                    ImGui::EndDisabled();
                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Tonemap", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGui::Checkbox("Dither", &Post.bTonemapDither);
                    ImGui::TreePop();
                }
            }
            return bChanged;
        }

        template<typename LightSetting>
        bool EditLight(LightSetting& Light)
        {
            bool bChanged = false;
            if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::TreeNodeEx("Shadows", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGui::Checkbox("Directional", &Light.bDirectionalShadow);
                    ImGui::BeginDisabled(!Light.bDirectionalShadow);
                    bChanged |= Slider("Normal Bias", &Light.ShadowNormalBias, 0.0F, 0.2F);
                    ImGui::EndDisabled();
                    bChanged |= ImGui::Checkbox("Point", &Light.bPointShadow);
                    ImGui::BeginDisabled(!Light.bPointShadow);
                    bChanged |= Slider("Depth Bias", &Light.PointShadowDepthBias, 0.0F, 0.05F);
                    ImGui::EndDisabled();
                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Image Based Lighting", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGui::Checkbox("Enable", &Light.bImageBasedLighting);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Sky", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGui::Checkbox("Enable", &Light.bSkyBackground);
                    ImGui::TreePop();
                }
            }
            return bChanged;
        }

        template<typename InsightSetting>
        bool EditInsight(InsightSetting& Insight)
        {
            bool bChanged = false;
            if (ImGui::CollapsingHeader("Insight", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::TreeNodeEx("FPS", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bChanged |= ImGui::Checkbox("Enable", &Insight.bShowFrameRate);
                    ImGui::TreePop();
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
            ImGui::End();
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
            ImGui::End();
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
            ImGui::End();
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
            ImGui::End();
        }
    }

    void BuildFrameRateReadout()
    {
        if (!UIManager::Get().ShowsFrameRate())
        {
            return;
        }

        const ImGuiViewport* Viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(Viewport->WorkPos.x + 12.0F, Viewport->WorkPos.y + 12.0F), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.45F);
        constexpr ImGuiWindowFlags WindowFlags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("Frame Rate", nullptr, WindowFlags))
        {
            const int Rate = static_cast<int>(ImGui::GetIO().Framerate + 0.5F);
            ImGui::Text("%d", Rate);
        }
        ImGui::End();
    }
}
