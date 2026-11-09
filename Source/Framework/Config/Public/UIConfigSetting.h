#pragma once

namespace ShadowEngine
{
    namespace UIConfigSetting
    {
        struct DeferredPostUIParameter
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

        struct DeferredLightUIParameter
        {
            bool bDirectionalShadow = true;
            bool bPointShadow = true;
            float ShadowNormalBias = 0.01F;
            float PointShadowDepthBias = 0.004F;
            bool bImageBasedLighting = true;
            bool bSkyBackground = true;
        };

        struct InsightUIParameter
        {
            bool bShowFrameRate = true;
        };

        struct DeferredUIParameter
        {
            InsightUIParameter Frame;
            DeferredLightUIParameter Light;
            DeferredPostUIParameter Post;
        };

        struct ForwardPostUIParameter
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

        struct ForwardLightUIParameter
        {
            bool bDirectionalShadow = true;
            bool bPointShadow = true;
            float ShadowNormalBias = 0.01F;
            float PointShadowDepthBias = 0.004F;
            bool bImageBasedLighting = true;
            bool bSkyBackground = true;
        };

        struct ForwardUIParameter
        {
            InsightUIParameter Frame;
            ForwardLightUIParameter Light;
            ForwardPostUIParameter Post;
        };

        struct BlinnPhongUIParameter
        {
            InsightUIParameter Frame;
        };

        struct DebugUIParameter
        {
            InsightUIParameter Frame;
        };
    }
}
