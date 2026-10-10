#pragma once

#include "Framework/RHI/Public/RHIEnums.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <format>
#include <string>

namespace ShadowEngine
{
    using Microsoft::WRL::ComPtr;

    inline constexpr D3D_FEATURE_LEVEL D3D12MinimumFeatureLevel = D3D_FEATURE_LEVEL_12_0;
    inline constexpr D3D_SHADER_MODEL D3D12MinimumShaderModel = static_cast<D3D_SHADER_MODEL>(0x68);

    inline std::string FormatHResult(HRESULT Result)
    {
        return std::format("HRESULT 0x{:08X}", static_cast<unsigned long>(Result));
    }

    inline std::string WideToUtf8(const wchar_t* Text)
    {
        const int Size = WideCharToMultiByte(CP_UTF8, 0, Text, -1, nullptr, 0, nullptr, nullptr);
        if (Size <= 1)
        {
            return {};
        }

        std::string Result(static_cast<size_t>(Size - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, Text, -1, Result.data(), Size, nullptr, nullptr);
        return Result;
    }

    inline DXGI_FORMAT ToDXGIFormat(ERHIFormat Format)
    {
        switch (Format)
        {
            case ERHIFormat::R8G8B8A8_UNorm:
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            case ERHIFormat::R32G32B32A32_Float:
                return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case ERHIFormat::D32_Float:
                return DXGI_FORMAT_D32_FLOAT;
            case ERHIFormat::Unknown:
                break;
        }

        return DXGI_FORMAT_UNKNOWN;
    }
}
