#include "Framework/Config/Public/UIConfigManager.h"

namespace ShadowEngine
{
    namespace
    {
        float ClampFloat(float Value, float Minimum, float Maximum)
        {
            if (Value < Minimum)
            {
                return Minimum;
            }
            if (Value > Maximum)
            {
                return Maximum;
            }
            return Value;
        }

        template<typename PostSetting>
        void ClampPost(PostSetting& Setting)
        {
            Setting.TAACurrentWeight = ClampFloat(Setting.TAACurrentWeight, 0.01F, 1.0F);
            Setting.ExposureMiddleGrey = ClampFloat(Setting.ExposureMiddleGrey, 0.01F, 1.0F);
            Setting.ExposureMinimum = ClampFloat(Setting.ExposureMinimum, 0.001F, 4.0F);
            Setting.ExposureMaximum = ClampFloat(Setting.ExposureMaximum, 0.05F, 32.0F);
            if (Setting.ExposureMinimum > Setting.ExposureMaximum)
            {
                Setting.ExposureMaximum = Setting.ExposureMinimum;
            }
            Setting.ExposureAdaptSpeed = ClampFloat(Setting.ExposureAdaptSpeed, 0.0F, 10.0F);
            Setting.BloomThreshold = ClampFloat(Setting.BloomThreshold, 0.0F, 8.0F);
            Setting.BloomStrength = ClampFloat(Setting.BloomStrength, 0.0F, 1.0F);
            Setting.BloomLimit = ClampFloat(Setting.BloomLimit, 0.5F, 16.0F);
        }

        template<typename LightSetting>
        void ClampLight(LightSetting& Setting)
        {
            Setting.ShadowNormalBias = ClampFloat(Setting.ShadowNormalBias, 0.0F, 0.2F);
            Setting.PointShadowDepthBias = ClampFloat(Setting.PointShadowDepthBias, 0.0F, 0.05F);
        }
    }

    void UIConfigManager::SubmitDeferred(UIConfigSetting::DeferredUIParameter Setting)
    {
        ClampPost(Setting.Post);
        ClampLight(Setting.Light);
        DeferredSetting = Setting;
    }

    void UIConfigManager::SubmitForward(UIConfigSetting::ForwardUIParameter Setting)
    {
        ClampPost(Setting.Post);
        ClampLight(Setting.Light);
        ForwardSetting = Setting;
    }

    UIConfigSetting::DeferredUIParameter UIConfigManager::GetDeferred() const
    {
        return DeferredSetting;
    }

    UIConfigSetting::ForwardUIParameter UIConfigManager::GetForward() const
    {
        return ForwardSetting;
    }

    void UIConfigManager::SubmitBlinnPhong(UIConfigSetting::BlinnPhongUIParameter Setting)
    {
        BlinnPhongSetting = Setting;
    }

    void UIConfigManager::SubmitDebug(UIConfigSetting::DebugUIParameter Setting)
    {
        DebugSetting = Setting;
    }

    UIConfigSetting::BlinnPhongUIParameter UIConfigManager::GetBlinnPhong() const
    {
        return BlinnPhongSetting;
    }

    UIConfigSetting::DebugUIParameter UIConfigManager::GetDebug() const
    {
        return DebugSetting;
    }
}
