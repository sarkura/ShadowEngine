#pragma once

#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHIDefinitions.h"
#include "RHI/Direct3D12/Public/D3D12Common.h"

#include <string>

namespace ShadowEngine
{
    class D3D12Buffer final : public RHIBuffer
    {
        public:
            bool InitializeVertex(
                ID3D12Device* Device,
                const RHIBufferDesc& Desc,
                std::string* ErrorMessage = nullptr);

            bool InitializeIndex(
                ID3D12Device* Device,
                const RHIBufferDesc& Desc,
                ERHIIndexFormat Format,
                std::string* ErrorMessage = nullptr);

            [[nodiscard]] uint32 GetSize() const override;
            [[nodiscard]] uint32 GetStride() const override;
            [[nodiscard]] D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
            [[nodiscard]] D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const;

        private:
            bool CreateUploadBuffer(
                ID3D12Device* Device,
                const RHIBufferDesc& Desc,
                std::string* ErrorMessage);

            ComPtr<ID3D12Resource> Resource;
            uint32 Size = 0;
            uint32 Stride = 0;
            DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
    };
}
