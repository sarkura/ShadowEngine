#pragma once

#include "Framework/RHI/Public/RHIEnums.h"

#include <span>
#include <string>

namespace ShadowEngine
{
    class RHISampler;
    class RHIShader;
    class RHITexture;

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

    struct RHIBufferDesc
    {
        uint32 Stride = 0;
        std::span<const uint8> Data;
    };

    struct RHITextureMipDesc
    {
        uint32 Width = 0;
        uint32 Height = 0;
        const uint8* Pixels = nullptr;
        uint32 Size = 0;
    };

    struct RHITextureDesc
    {
        ERHIFormat Format = ERHIFormat::R8G8B8A8_UNorm;
        std::span<const RHITextureMipDesc> Mips;
    };

    struct RHIMaterialBindingDesc
    {
        RHITexture* BaseColor = nullptr;
        RHITexture* Roughness = nullptr;
        RHITexture* Normal = nullptr;
        RHISampler* Sampler = nullptr;
    };

    struct RHIInputElement
    {
        const char* Semantic = "";
        uint32 SemanticIndex = 0;
        uint32 Offset = 0;
        ERHIVertexFormat Format = ERHIVertexFormat::Float32x3;
    };

    struct RHIGraphicsPipelineDesc
    {
        RHIShader* VertexShader = nullptr;
        RHIShader* PixelShader = nullptr;
        ERHIFormat RenderTargetFormat = ERHIFormat::R8G8B8A8_UNorm;
        ERHIPrimitiveTopology Topology = ERHIPrimitiveTopology::TriangleList;
        std::span<const RHIInputElement> InputLayout;
        bool bEnableDepth = false;
        ERHIFormat DepthFormat = ERHIFormat::Unknown;
    };
}
