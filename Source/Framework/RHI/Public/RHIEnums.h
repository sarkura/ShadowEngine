#pragma once

#include "Framework/Common/Public/Types.h"

namespace ShadowEngine
{
    enum class ERHIFormat : uint8
    {
        Unknown,
        R8G8B8A8_UNorm,
        D32_Float
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

    enum class ERHIVertexFormat : uint8
    {
        Float32x2,
        Float32x3
    };

    enum class ERHIIndexFormat : uint8
    {
        Uint16,
        Uint32
    };
}
