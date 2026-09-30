#pragma once

#include "Framework/RHI/Public/RHIEnums.h"

#include <span>
#include <string>

namespace ShadowEngine
{
    class RHIShader;

    struct RHIDeviceDesc
    {
        bool bEnableDebugLayer = false;
    };

    struct RHISwapChainDesc
    {
        void* WindowHandle = nullptr;
        uint32 Width = 0;
        uint32 Height = 0;
        uint32 BufferCount = 2;
        ERHIFormat Format = ERHIFormat::R8G8B8A8_UNorm;
        bool bVSync = true;
    };

    struct RHIShaderDesc
    {
        ERHIShaderStage Stage = ERHIShaderStage::Vertex;
        std::string EntryPoint;
        std::span<const uint8> Bytecode;
    };

    struct RHIGraphicsPipelineDesc
    {
        RHIShader* VertexShader = nullptr;
        RHIShader* PixelShader = nullptr;
        ERHIFormat RenderTargetFormat = ERHIFormat::R8G8B8A8_UNorm;
        ERHIPrimitiveTopology Topology = ERHIPrimitiveTopology::TriangleList;
    };
}
