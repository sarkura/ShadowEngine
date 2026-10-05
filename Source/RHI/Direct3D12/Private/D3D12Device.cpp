#include "RHI/Direct3D12/Public/D3D12Device.h"

#include "Framework/Common/Public/Log.h"
#include "RHI/Direct3D12/Public/D3D12Buffer.h"
#include "RHI/Direct3D12/Public/D3D12CommandList.h"
#include "RHI/Direct3D12/Public/D3D12Pipeline.h"
#include "RHI/Direct3D12/Public/D3D12Shader.h"
#include "RHI/Direct3D12/Public/D3D12Texture.h"
#include "RHI/Direct3D12/Public/D3D12SwapChain.h"

#include <string_view>
#include <vector>

namespace ShadowEngine
{
    D3D12Device::~D3D12Device()
    {
        Finalize();
    }

    bool D3D12Device::Initialize(const RHIDeviceDesc& Desc, std::string* ErrorMessage)
    {
        if (!CreateFactory(Desc.bEnableDebugLayer, ErrorMessage))
        {
            return false;
        }

        if (!Adapter.Initialize(Factory.Get(), ErrorMessage))
        {
            return false;
        }

        HRESULT Result = D3D12CreateDevice(
            Adapter.GetHandle(),
            D3D12MinimumFeatureLevel,
            IID_PPV_ARGS(Device.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "D3D12CreateDevice failed: " + FormatHResult(Result));
            return false;
        }

        D3D12_FEATURE_DATA_SHADER_MODEL ShaderModel{D3D12MinimumShaderModel};
        if (FAILED(Device->CheckFeatureSupport(
                D3D12_FEATURE_SHADER_MODEL,
                &ShaderModel,
                sizeof(ShaderModel))) ||
            ShaderModel.HighestShaderModel < D3D12MinimumShaderModel)
        {
            SetErrorMessage(
                ErrorMessage,
                "The adapter or its driver does not support Shader Model 6.0, which is required for DXIL");
            return false;
        }

        if (Desc.bEnableDebugLayer)
        {
            Device.As(&InfoQueue);
        }

        D3D12_COMMAND_QUEUE_DESC QueueDesc{};
        QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        Result = Device->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(Queue.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateCommandQueue failed: " + FormatHResult(Result));
            return false;
        }

        Fence = std::make_unique<D3D12Fence>();
        return Fence->Initialize(Device.Get(), ErrorMessage);
    }

    bool D3D12Device::CreateFactory(bool bEnableDebugLayer, std::string* ErrorMessage)
    {
        UINT FactoryFlags = 0;
        if (bEnableDebugLayer)
        {
            ComPtr<ID3D12Debug> Debug;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(Debug.GetAddressOf()))))
            {
                Debug->EnableDebugLayer();
                FactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
                Log::Info("D3D12 debug layer enabled");
            }
            else
            {
                Log::Warning("D3D12 debug layer is not available, install the Graphics Tools feature to use it");
            }
        }

        HRESULT Result = CreateDXGIFactory2(FactoryFlags, IID_PPV_ARGS(Factory.GetAddressOf()));
        if (FAILED(Result) && FactoryFlags != 0)
        {
            Log::Warning("DXGI debug factory is not available, falling back to the release factory");
            Result = CreateDXGIFactory2(0, IID_PPV_ARGS(Factory.GetAddressOf()));
        }

        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateDXGIFactory2 failed: " + FormatHResult(Result));
            return false;
        }

        return true;
    }

    void D3D12Device::Finalize()
    {
        if (Queue != nullptr && Fence != nullptr)
        {
            WaitIdle();
        }

        FlushDebugMessages();

        Fence.reset();
        Queue.Reset();
        InfoQueue.Reset();
        Device.Reset();
        Factory.Reset();
    }

    std::string_view D3D12Device::GetName() const
    {
        return "D3D12";
    }

    ERHIShaderFormat D3D12Device::GetShaderFormat() const
    {
        return ERHIShaderFormat::DXIL;
    }

    const RHIAdapter* D3D12Device::GetAdapter() const
    {
        return &Adapter;
    }

    std::unique_ptr<RHISwapChain> D3D12Device::CreateSwapChain(
        const RHISwapChainDesc& Desc,
        std::string* ErrorMessage)
    {
        auto SwapChain = std::make_unique<D3D12SwapChain>();
        if (!SwapChain->Initialize(Factory.Get(), Device.Get(), Queue.Get(), Desc, ErrorMessage))
        {
            return nullptr;
        }

        return SwapChain;
    }

    std::unique_ptr<RHIShader> D3D12Device::CreateShader(
        const RHIShaderDesc& Desc,
        std::string* ErrorMessage)
    {
        if (Desc.Bytecode.empty())
        {
            SetErrorMessage(ErrorMessage, "Shader bytecode is empty: " + Desc.EntryPoint);
            return nullptr;
        }

        return std::make_unique<D3D12Shader>(Desc);
    }

    std::unique_ptr<RHIPipeline> D3D12Device::CreateGraphicsPipeline(
        const RHIGraphicsPipelineDesc& Desc,
        std::string* ErrorMessage)
    {
        auto Pipeline = std::make_unique<D3D12Pipeline>();
        if (!Pipeline->Initialize(Device.Get(), Desc, ErrorMessage))
        {
            return nullptr;
        }

        return Pipeline;
    }

    std::unique_ptr<RHIBuffer> D3D12Device::CreateVertexBuffer(
        const RHIBufferDesc& Desc,
        std::string* ErrorMessage)
    {
        auto Buffer = std::make_unique<D3D12Buffer>();
        if (!Buffer->InitializeVertex(Device.Get(), Desc, ErrorMessage))
        {
            return nullptr;
        }

        return Buffer;
    }

    std::unique_ptr<RHIBuffer> D3D12Device::CreateIndexBuffer(
        const RHIBufferDesc& Desc,
        ERHIIndexFormat Format,
        std::string* ErrorMessage)
    {
        auto Buffer = std::make_unique<D3D12Buffer>();
        if (!Buffer->InitializeIndex(Device.Get(), Desc, Format, ErrorMessage))
        {
            return nullptr;
        }

        return Buffer;
    }

    std::unique_ptr<RHIBuffer> D3D12Device::CreateConstantBuffer(
        uint32 Size,
        std::string* ErrorMessage)
    {
        auto Buffer = std::make_unique<D3D12Buffer>();
        if (!Buffer->InitializeConstant(Device.Get(), Size, ErrorMessage))
        {
            return nullptr;
        }

        return Buffer;
    }

    std::unique_ptr<RHITexture> D3D12Device::CreateDepthTexture(
        uint32 Width,
        uint32 Height,
        std::string* ErrorMessage)
    {
        return D3D12Texture::CreateDepth(Device.Get(), Width, Height, ErrorMessage);
    }

    std::unique_ptr<RHICommandList> D3D12Device::CreateCommandList(std::string* ErrorMessage)
    {
        auto CommandList = std::make_unique<D3D12CommandList>();
        if (!CommandList->Initialize(Device.Get(), ErrorMessage))
        {
            return nullptr;
        }

        return CommandList;
    }

    void D3D12Device::SubmitCommandList(RHICommandList& CommandList)
    {
        ID3D12CommandList* Lists[] = {static_cast<D3D12CommandList&>(CommandList).GetHandle()};
        Queue->ExecuteCommandLists(1, Lists);
    }

    void D3D12Device::WaitIdle()
    {
        if (Queue == nullptr || Fence == nullptr)
        {
            return;
        }

        Fence->Wait(Fence->Signal(Queue.Get()));
        FlushDebugMessages();
    }

    void D3D12Device::FlushDebugMessages()
    {
        if (InfoQueue == nullptr)
        {
            return;
        }

        const UINT64 Count = InfoQueue->GetNumStoredMessages();
        for (UINT64 Index = 0; Index < Count; ++Index)
        {
            SIZE_T Length = 0;
            if (FAILED(InfoQueue->GetMessage(Index, nullptr, &Length)) || Length == 0)
            {
                continue;
            }

            std::vector<char> Storage(Length);
            auto* Message = reinterpret_cast<D3D12_MESSAGE*>(Storage.data());
            if (FAILED(InfoQueue->GetMessage(Index, Message, &Length)))
            {
                continue;
            }

            std::string_view Text(Message->pDescription, Message->DescriptionByteLength);
            if (!Text.empty() && Text.back() == '\0')
            {
                Text.remove_suffix(1);
            }
            switch (Message->Severity)
            {
                case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                case D3D12_MESSAGE_SEVERITY_ERROR:
                    Log::Error("D3D12: {}", Text);
                    break;
                case D3D12_MESSAGE_SEVERITY_WARNING:
                    Log::Warning("D3D12: {}", Text);
                    break;
                default:
                    break;
            }
        }

        InfoQueue->ClearStoredMessages();
    }
}
