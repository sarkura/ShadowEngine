#include "RHI/Direct3D12/Public/D3D12Pipeline.h"

#include "Framework/Common/Public/Log.h"
#include "RHI/Direct3D12/Public/D3D12Shader.h"

#include <climits>
#include <vector>

namespace ShadowEngine
{
    namespace
    {
        D3D12_BLEND_DESC MakeOpaqueBlendDesc()
        {
            D3D12_BLEND_DESC Desc{};
            Desc.AlphaToCoverageEnable = FALSE;
            Desc.IndependentBlendEnable = FALSE;

            D3D12_RENDER_TARGET_BLEND_DESC& Target = Desc.RenderTarget[0];
            Target.BlendEnable = FALSE;
            Target.LogicOpEnable = FALSE;
            Target.SrcBlend = D3D12_BLEND_ONE;
            Target.DestBlend = D3D12_BLEND_ZERO;
            Target.BlendOp = D3D12_BLEND_OP_ADD;
            Target.SrcBlendAlpha = D3D12_BLEND_ONE;
            Target.DestBlendAlpha = D3D12_BLEND_ZERO;
            Target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
            Target.LogicOp = D3D12_LOGIC_OP_NOOP;
            Target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            return Desc;
        }

        D3D12_RASTERIZER_DESC MakeRasterizerDesc()
        {
            D3D12_RASTERIZER_DESC Desc{};
            Desc.FillMode = D3D12_FILL_MODE_SOLID;
            Desc.CullMode = D3D12_CULL_MODE_NONE;
            Desc.FrontCounterClockwise = FALSE;
            Desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
            Desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
            Desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
            Desc.DepthClipEnable = TRUE;
            Desc.MultisampleEnable = FALSE;
            Desc.AntialiasedLineEnable = FALSE;
            Desc.ForcedSampleCount = 0;
            Desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
            return Desc;
        }

        D3D12_DEPTH_STENCIL_DESC MakeDepthStencilDesc(bool bEnableDepth)
        {
            D3D12_DEPTH_STENCIL_DESC Desc{};
            Desc.DepthEnable = bEnableDepth ? TRUE : FALSE;
            Desc.DepthWriteMask = bEnableDepth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
            Desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
            Desc.StencilEnable = FALSE;
            return Desc;
        }

        D3D12_PRIMITIVE_TOPOLOGY_TYPE ToTopologyType(ERHIPrimitiveTopology Topology)
        {
            switch (Topology)
            {
                case ERHIPrimitiveTopology::TriangleList:
                    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            }

            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
        }

        D3D_PRIMITIVE_TOPOLOGY ToTopology(ERHIPrimitiveTopology Topology)
        {
            switch (Topology)
            {
                case ERHIPrimitiveTopology::TriangleList:
                    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            }

            return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        }

        DXGI_FORMAT ToVertexFormat(ERHIVertexFormat Format)
        {
            switch (Format)
            {
                case ERHIVertexFormat::Float32x2:
                    return DXGI_FORMAT_R32G32_FLOAT;
                case ERHIVertexFormat::Float32x3:
                    return DXGI_FORMAT_R32G32B32_FLOAT;
            }

            return DXGI_FORMAT_UNKNOWN;
        }
    }

    bool D3D12Pipeline::Initialize(
        ID3D12Device* Device,
        const RHIGraphicsPipelineDesc& Desc,
        std::string* ErrorMessage)
    {
        if (Desc.VertexShader == nullptr || Desc.PixelShader == nullptr)
        {
            SetErrorMessage(ErrorMessage, "Graphics pipeline requires a vertex and a pixel shader");
            return false;
        }

        if (!CreateRootSignature(Device, ErrorMessage))
        {
            return false;
        }

        const auto* VertexShader = static_cast<const D3D12Shader*>(Desc.VertexShader);
        const auto* PixelShader = static_cast<const D3D12Shader*>(Desc.PixelShader);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineDesc{};
        PipelineDesc.pRootSignature = RootSignature.Get();
        PipelineDesc.VS = VertexShader->GetBytecode();
        PipelineDesc.PS = PixelShader->GetBytecode();
        PipelineDesc.BlendState = MakeOpaqueBlendDesc();
        PipelineDesc.SampleMask = UINT_MAX;
        PipelineDesc.RasterizerState = MakeRasterizerDesc();
        PipelineDesc.DepthStencilState = MakeDepthStencilDesc(Desc.bEnableDepth);

        std::vector<D3D12_INPUT_ELEMENT_DESC> InputElements;
        InputElements.reserve(Desc.InputLayout.size());
        for (const RHIInputElement& Element : Desc.InputLayout)
        {
            D3D12_INPUT_ELEMENT_DESC Input{};
            Input.SemanticName = Element.Semantic;
            Input.SemanticIndex = Element.SemanticIndex;
            Input.Format = ToVertexFormat(Element.Format);
            Input.InputSlot = 0;
            Input.AlignedByteOffset = Element.Offset;
            Input.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            Input.InstanceDataStepRate = 0;
            InputElements.push_back(Input);
        }
        PipelineDesc.InputLayout.pInputElementDescs = InputElements.data();
        PipelineDesc.InputLayout.NumElements = static_cast<UINT>(InputElements.size());
        PipelineDesc.PrimitiveTopologyType = ToTopologyType(Desc.Topology);
        PipelineDesc.NumRenderTargets = 1;
        PipelineDesc.RTVFormats[0] = ToDXGIFormat(Desc.RenderTargetFormat);
        if (Desc.DepthFormat != ERHIFormat::Unknown)
        {
            PipelineDesc.DSVFormat = ToDXGIFormat(Desc.DepthFormat);
        }
        else
        {
            PipelineDesc.DSVFormat = Desc.bEnableDepth ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
        }
        PipelineDesc.SampleDesc.Count = 1;
        PipelineDesc.SampleDesc.Quality = 0;

        const HRESULT Result = Device->CreateGraphicsPipelineState(
            &PipelineDesc,
            IID_PPV_ARGS(PipelineState.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(
                ErrorMessage,
                "CreateGraphicsPipelineState failed: " + FormatHResult(Result));
            return false;
        }

        Topology = ToTopology(Desc.Topology);
        return true;
    }

    bool D3D12Pipeline::CreateRootSignature(ID3D12Device* Device, std::string* ErrorMessage)
    {
        D3D12_ROOT_PARAMETER Parameters[1]{};
        Parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        Parameters[0].Descriptor.ShaderRegister = 0;
        Parameters[0].Descriptor.RegisterSpace = 0;
        Parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC Desc{};
        Desc.NumParameters = 1;
        Desc.pParameters = Parameters;
        Desc.NumStaticSamplers = 0;
        Desc.pStaticSamplers = nullptr;
        Desc.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
            D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

        ComPtr<ID3DBlob> Signature;
        ComPtr<ID3DBlob> Error;
        HRESULT Result = D3D12SerializeRootSignature(
            &Desc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            Signature.GetAddressOf(),
            Error.GetAddressOf());
        if (FAILED(Result))
        {
            std::string Message = "D3D12SerializeRootSignature failed: " + FormatHResult(Result);
            if (Error != nullptr)
            {
                Message += " ";
                Message.append(
                    static_cast<const char*>(Error->GetBufferPointer()),
                    Error->GetBufferSize());
            }
            SetErrorMessage(ErrorMessage, std::move(Message));
            return false;
        }

        Result = Device->CreateRootSignature(
            0,
            Signature->GetBufferPointer(),
            Signature->GetBufferSize(),
            IID_PPV_ARGS(RootSignature.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateRootSignature failed: " + FormatHResult(Result));
            return false;
        }

        return true;
    }

    ID3D12RootSignature* D3D12Pipeline::GetRootSignature() const
    {
        return RootSignature.Get();
    }

    ID3D12PipelineState* D3D12Pipeline::GetPipelineState() const
    {
        return PipelineState.Get();
    }

    D3D_PRIMITIVE_TOPOLOGY D3D12Pipeline::GetTopology() const
    {
        return Topology;
    }
}
