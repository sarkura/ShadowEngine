#include "RHI/Direct3D12/Public/D3D12RHIModule.h"

#include "Framework/Engine/Public/EngineModules.h"
#include "RHI/Direct3D12/Public/D3D12Device.h"

namespace ShadowEngine
{
    void RegisterD3D12RHI()
    {
        EngineModules::Get().RegisterRHI("D3D12", []
        {
            return std::make_unique<D3D12Device>();
        });
    }
}
