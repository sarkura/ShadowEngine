#pragma once

#include "Framework/Common/Public/Types.h"

namespace ShadowEngine
{
    enum class ERHIFormat : uint8
    {
        Unknown,
        R8G8B8A8_UNorm
    };

    enum class ERHIShaderStage : uint8
    {
        Vertex,
        Pixel
    };

    enum class ERHIShaderFormat : uint8
    {
        DXBC,
        DXIL,
        SPIRV
    };

    enum class ERHIPrimitiveTopology : uint8
    {
        TriangleList
    };
}
