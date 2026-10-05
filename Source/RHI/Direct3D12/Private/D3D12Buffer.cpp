#include "RHI/Direct3D12/Public/D3D12Buffer.h"

#include "Framework/Common/Public/Log.h"

#include <cstring>

namespace ShadowEngine
{
    namespace
    {
        DXGI_FORMAT ToDXGIIndexFormat(ERHIIndexFormat Format)
        {
            switch (Format)
            {
                case ERHIIndexFormat::Uint16:
                    return DXGI_FORMAT_R16_UINT;
                case ERHIIndexFormat::Uint32:
                    return DXGI_FORMAT_R32_UINT;
            }

            return DXGI_FORMAT_UNKNOWN;
        }

        uint32 IndexStride(ERHIIndexFormat Format)
        {
            switch (Format)
            {
                case ERHIIndexFormat::Uint16:
                    return sizeof(uint16);
                case ERHIIndexFormat::Uint32:
                    return sizeof(uint32);
            }

            return 0;
        }
    }

    bool D3D12Buffer::InitializeVertex(
        ID3D12Device* Device,
        const RHIBufferDesc& Desc,
        std::string* ErrorMessage)
    {
        if (Desc.Stride == 0 || Desc.Data.size() % Desc.Stride != 0)
        {
            SetErrorMessage(ErrorMessage, "Vertex buffer stride does not match its data");
            return false;
        }

        Stride = Desc.Stride;
        return CreateUploadBuffer(Device, Desc, ErrorMessage);
    }

    bool D3D12Buffer::InitializeIndex(
        ID3D12Device* Device,
        const RHIBufferDesc& Desc,
        ERHIIndexFormat Format,
        std::string* ErrorMessage)
    {
        const uint32 ElementStride = IndexStride(Format);
        if (ElementStride == 0 || Desc.Data.size() % ElementStride != 0)
        {
            SetErrorMessage(ErrorMessage, "Index buffer format does not match its data");
            return false;
        }

        Stride = ElementStride;
        IndexFormat = ToDXGIIndexFormat(Format);
        return CreateUploadBuffer(Device, Desc, ErrorMessage);
    }

    bool D3D12Buffer::InitializeConstant(
        ID3D12Device* Device,
        uint32 InSize,
        std::string* ErrorMessage)
    {
        if (InSize == 0 || InSize % 256 != 0)
        {
            SetErrorMessage(ErrorMessage, "Constant buffer size must be a non-zero multiple of 256");
            return false;
        }

        D3D12_HEAP_PROPERTIES HeapProperties{};
        HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        HeapProperties.CreationNodeMask = 1;
        HeapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC ResourceDesc{};
        ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        ResourceDesc.Width = InSize;
        ResourceDesc.Height = 1;
        ResourceDesc.DepthOrArraySize = 1;
        ResourceDesc.MipLevels = 1;
        ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        ResourceDesc.SampleDesc.Count = 1;
        ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        const HRESULT Result = Device->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &ResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(Resource.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateCommittedResource failed: " + FormatHResult(Result));
            return false;
        }

        void* Mapped = nullptr;
        if (FAILED(Resource->Map(0, nullptr, &Mapped)))
        {
            SetErrorMessage(ErrorMessage, "ID3D12Resource::Map failed for the constant buffer");
            return false;
        }

        MappedData = static_cast<uint8*>(Mapped);
        std::memset(MappedData, 0, InSize);
        Size = InSize;
        Stride = 256;
        return true;
    }

    D3D12Buffer::~D3D12Buffer()
    {
        if (Resource != nullptr && MappedData != nullptr)
        {
            Resource->Unmap(0, nullptr);
            MappedData = nullptr;
        }
    }

    bool D3D12Buffer::CreateUploadBuffer(
        ID3D12Device* Device,
        const RHIBufferDesc& Desc,
        std::string* ErrorMessage)
    {
        if (Desc.Data.empty())
        {
            SetErrorMessage(ErrorMessage, "Buffer data is empty");
            return false;
        }

        D3D12_HEAP_PROPERTIES HeapProperties{};
        HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        HeapProperties.CreationNodeMask = 1;
        HeapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC ResourceDesc{};
        ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        ResourceDesc.Width = Desc.Data.size();
        ResourceDesc.Height = 1;
        ResourceDesc.DepthOrArraySize = 1;
        ResourceDesc.MipLevels = 1;
        ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        ResourceDesc.SampleDesc.Count = 1;
        ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT Result = Device->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &ResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(Resource.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateCommittedResource failed: " + FormatHResult(Result));
            return false;
        }

        void* Mapped = nullptr;
        Result = Resource->Map(0, nullptr, &Mapped);
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "ID3D12Resource::Map failed: " + FormatHResult(Result));
            return false;
        }

        std::memcpy(Mapped, Desc.Data.data(), Desc.Data.size());
        Resource->Unmap(0, nullptr);

        Size = static_cast<uint32>(Desc.Data.size());
        return true;
    }

    uint32 D3D12Buffer::GetSize() const
    {
        return Size;
    }

    uint32 D3D12Buffer::GetStride() const
    {
        return Stride;
    }

    bool D3D12Buffer::Update(uint32 Offset, std::span<const uint8> Data)
    {
        if (MappedData == nullptr || Offset + Data.size() > Size)
        {
            return false;
        }

        std::memcpy(MappedData + Offset, Data.data(), Data.size());
        return true;
    }

    D3D12_GPU_VIRTUAL_ADDRESS D3D12Buffer::GetGPUVirtualAddress() const
    {
        return Resource->GetGPUVirtualAddress();
    }

    D3D12_VERTEX_BUFFER_VIEW D3D12Buffer::GetVertexBufferView() const
    {
        D3D12_VERTEX_BUFFER_VIEW View{};
        View.BufferLocation = Resource->GetGPUVirtualAddress();
        View.SizeInBytes = Size;
        View.StrideInBytes = Stride;
        return View;
    }

    D3D12_INDEX_BUFFER_VIEW D3D12Buffer::GetIndexBufferView() const
    {
        D3D12_INDEX_BUFFER_VIEW View{};
        View.BufferLocation = Resource->GetGPUVirtualAddress();
        View.SizeInBytes = Size;
        View.Format = IndexFormat;
        return View;
    }
}
