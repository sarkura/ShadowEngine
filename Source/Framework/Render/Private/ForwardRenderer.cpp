#include "Framework/Render/Public/ForwardRenderer.h"

#include "Framework/Asset/Public/AssetManager.h"
#include "Framework/Asset/Public/TextureAsset.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/RHI/Public/RHIDefinitions.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/RHI/Public/RHIPipelineLayout.h"
#include "Framework/RHI/Public/RHISwapChain.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <cstring>

namespace ShadowEngine
{
    namespace
    {
        constexpr uint32 ForwardPointLightLimit = 4;

        float Halton(uint32 Index, uint32 Base)
        {
            float Result = 0.0F;
            float Fraction = 1.0F;
            while (Index > 0)
            {
                Fraction /= static_cast<float>(Base);
                Result += Fraction * static_cast<float>(Index % Base);
                Index /= Base;
            }
            return Result;
        }
    }

    bool ForwardRenderer::RenderFrame()
    {
        if (Assets == nullptr || ShaderLibrary == nullptr || Device == nullptr || SwapChain == nullptr || DepthBuffer == nullptr)
        {
            return false;
        }

        if (!CollectVisibleItems(ItemFilter, DrawQueue))
        {
            return false;
        }

        std::string Error;
        if (DepthPrepassPipeline == nullptr)
        {
            const ShaderHandle DepthShader = ShaderLibrary->Register(
                "Depth/DepthPrepass.slang",
                "Depth/DepthPrepass.slang",
                {},
                {});
            if (!DepthShader.IsValid() || !ShaderLibrary->LoadShaders(*Device, DepthShader, &Error))
            {
                Log::Error("{}", Error);
                return false;
            }

            const ShaderReflection* VertexReflection = ShaderLibrary->GetReflection(DepthShader, ERHIShaderStage::Vertex);
            const ShaderReflection* PixelReflection = ShaderLibrary->GetReflection(DepthShader, ERHIShaderStage::Pixel);
            const RHIPipelineLayout Layout = VertexReflection != nullptr && PixelReflection != nullptr
                ? RHIPipelineLayout::FromReflection(*VertexReflection, *PixelReflection)
                : RHIPipelineLayout{};
            constexpr RHIInputElement InputLayout[] = {
                {"POSITION", 0, 0, ERHIVertexFormat::Float32x3},
            };
            RHIGraphicsPipelineDesc PipelineDesc;
            PipelineDesc.VertexShader = ShaderLibrary->GetVertexShader(DepthShader);
            PipelineDesc.PixelShader = ShaderLibrary->GetPixelShader(DepthShader);
            PipelineDesc.Layout = &Layout;
            PipelineDesc.RenderTargetFormat = ERHIFormat::R16G16B16A16_Float;
            PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
            PipelineDesc.InputLayout = InputLayout;
            PipelineDesc.bEnableDepth = true;
            PipelineDesc.bWriteDepth = true;
            PipelineDesc.bDepthEqual = false;
            PipelineDesc.bWriteColor = false;
            PipelineDesc.DepthFormat = ERHIFormat::D32_Float;
            DepthPrepassPipeline = Device->CreateGraphicsPipeline(PipelineDesc, &Error);
            if (DepthPrepassPipeline == nullptr)
            {
                Log::Error("{}", Error);
                return false;
            }
        }

        if (!EnvironmentCubeMap.IsReady())
        {
            const TextureAsset* Environment = Assets->ResolveTexture(Assets->GetEnvironmentMap());
            const uint32 SourceWidth = Environment != nullptr && !Environment->GetMips().empty()
                ? Environment->GetMips().front().Width
                : 0;
            const uint32 FaceSize = SourceWidth / 4;
            if (!EnvironmentCubeMap.Ensure(
                    *Device,
                    *ShaderLibrary,
                    *CommandList,
                    EnvironmentDescriptor,
                    EnvironmentSampler,
                    FaceSize,
                    &Error))
            {
                Log::Error("{}", Error);
                return false;
            }

            EnvironmentDescriptor = EnvironmentCubeMap.GetDescriptor();
            ReleaseUploadedTexture(Assets->GetEnvironmentMap());
            Log::Info("Environment cube built ({} per face) and the equirectangular source was released", FaceSize);
        }

        if (!Lighting.Ensure(*Device, *ShaderLibrary, *CommandList, EnvironmentDescriptor, EnvironmentSampler, &Error))
        {
            Log::Error("{}", Error);
            return false;
        }

        if (!Post.Ensure(*Device, *ShaderLibrary, *DepthBuffer, SwapChain->GetWidth(), SwapChain->GetHeight(), SwapChain->GetFormat(), &Error))
        {
            Log::Error("{}", Error);
            return false;
        }

        const float JitterX = Halton(FrameIndex, 2) - 0.5F;
        const float JitterY = Halton(FrameIndex, 3) - 0.5F;
        ++FrameIndex;
        if (FrameIndex == 0)
        {
            FrameIndex = 1;
        }

        RenderPassContext Context;
        Context.Device = Device;
        Context.SwapChain = SwapChain;
        Context.CommandList = CommandList.get();
        Context.ConstantBuffer = ConstantBuffer.get();
        Context.DepthBuffer = DepthBuffer.get();
        Context.Assets = Assets;
        Context.Shaders = ShaderLibrary;
        Context.View = &ViewCamera;
        Context.Batches = &MeshBatches;
        Context.TextureDescriptors = &TextureDescriptors;
        Context.Samplers = &GpuSamplers;
        std::vector<PointLightRenderProxy> VisiblePointLights;
        SelectVisiblePointLights(ForwardPointLightLimit, VisiblePointLights);
        Context.DirectLight = SceneProxy.bHasDirectLight ? &SceneProxy.DirectLight : nullptr;
        Context.PointLights = &VisiblePointLights;
        Context.SkyLight = SceneProxy.bHasSkyLight ? &SceneProxy.SkyLight : nullptr;
        Context.SkyPipeline = SkyPipeline.get();
        Context.DepthPrepassPipeline = DepthPrepassPipeline.get();
        Context.SkyConstants = SkyConstantBuffer.get();
        Context.ImageBasedLight = &Lighting.GetBinding();
        Context.EnvironmentDescriptor = EnvironmentDescriptor;
        Context.EnvironmentSampler = EnvironmentSampler;
        Context.SceneColor = Post.GetSceneColor();
        Context.Post = &Post;
        Context.ProjectionJitterX = JitterX;
        Context.ProjectionJitterY = JitterY;
        Context.PreviousViewProjection = bHasPrevious ? PreviousViewProjection : nullptr;
        Context.PreviousWorlds = bHasPrevious ? &PreviousWorlds : nullptr;
        Context.StoredViewProjection = PreviousViewProjection;
        Context.ClearColor = ClearColor;
        Context.ConstantCapacity = ConstantCapacity;
        Context.ConstantAlignment = ConstantAlignment;
        if (!Pass.Execute(DrawQueue, Context))
        {
            return false;
        }

        PreviousWorlds.resize(SceneProxy.Meshes.size() * 16);
        for (size_t Index = 0; Index < SceneProxy.Meshes.size(); ++Index)
        {
            std::memcpy(PreviousWorlds.data() + Index * 16, SceneProxy.Meshes[Index].World, sizeof(float) * 16);
        }
        bHasPrevious = true;
        return true;
    }

    const char* ForwardRenderer::GetName() const
    {
        return "ForwardRenderer";
    }
}
