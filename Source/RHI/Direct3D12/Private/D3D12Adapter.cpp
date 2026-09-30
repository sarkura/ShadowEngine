#include "RHI/Direct3D12/Public/D3D12Adapter.h"

#include "Framework/Common/Public/Log.h"

#include <utility>

namespace ShadowEngine
{
    bool D3D12Adapter::Initialize(IDXGIFactory4* Factory, std::string* ErrorMessage)
    {
        ComPtr<IDXGIFactory6> Factory6;
        if (SUCCEEDED(Factory->QueryInterface(IID_PPV_ARGS(Factory6.GetAddressOf()))))
        {
            for (UINT Index = 0;; ++Index)
            {
                ComPtr<IDXGIAdapter1> Candidate;
                if (Factory6->EnumAdapterByGpuPreference(
                        Index,
                        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                        IID_PPV_ARGS(Candidate.GetAddressOf())) == DXGI_ERROR_NOT_FOUND)
                {
                    break;
                }

                if (TrySelect(std::move(Candidate)))
                {
                    return true;
                }
            }
        }

        for (UINT Index = 0;; ++Index)
        {
            ComPtr<IDXGIAdapter1> Candidate;
            if (Factory->EnumAdapters1(Index, Candidate.GetAddressOf()) == DXGI_ERROR_NOT_FOUND)
            {
                break;
            }

            if (TrySelect(std::move(Candidate)))
            {
                return true;
            }
        }

        SetErrorMessage(
            ErrorMessage,
            "No hardware adapter supports Direct3D 12 with feature level 12_0");
        return false;
    }

    bool D3D12Adapter::TrySelect(ComPtr<IDXGIAdapter1> Candidate)
    {
        DXGI_ADAPTER_DESC1 Desc{};
        if (FAILED(Candidate->GetDesc1(&Desc)) ||
            (Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
        {
            return false;
        }

        if (FAILED(D3D12CreateDevice(
                Candidate.Get(),
                D3D12MinimumFeatureLevel,
                __uuidof(ID3D12Device),
                nullptr)))
        {
            return false;
        }

        Adapter = std::move(Candidate);
        Info.Name = WideToUtf8(Desc.Description);
        Info.DedicatedVideoMemory = Desc.DedicatedVideoMemory;
        return true;
    }

    const RHIAdapterInfo& D3D12Adapter::GetInfo() const
    {
        return Info;
    }

    IDXGIAdapter1* D3D12Adapter::GetHandle() const
    {
        return Adapter.Get();
    }
}
