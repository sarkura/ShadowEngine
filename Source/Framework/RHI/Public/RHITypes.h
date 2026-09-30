#pragma once

#include "Framework/Common/Public/Types.h"

namespace ShadowEngine
{
    struct RHIColor
    {
        float R = 0.0F;
        float G = 0.0F;
        float B = 0.0F;
        float A = 1.0F;
    };

    struct RHIViewport
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Width = 0.0F;
        float Height = 0.0F;
        float MinDepth = 0.0F;
        float MaxDepth = 1.0F;
    };

    struct RHIRect
    {
        int32 Left = 0;
        int32 Top = 0;
        int32 Right = 0;
        int32 Bottom = 0;
    };
}
