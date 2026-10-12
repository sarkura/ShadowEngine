#pragma once

float3 SrgbToLinear(float3 Color)
{
    const float3 Low = Color / 12.92;
    const float3 High = pow(max((Color + 0.055) / 1.055, 0.0), 2.4);
    return lerp(Low, High, step(0.04045, Color));
}

float3 LinearToSrgb(float3 Color)
{
    const float3 Low = Color * 12.92;
    const float3 High = 1.055 * pow(max(Color, 0.0), 1.0 / 2.4) - 0.055;
    return lerp(Low, High, step(0.0031308, Color));
}
