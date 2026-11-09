#include "Framework/Render/Public/DeferredRenderer.h"

#include "Framework/Asset/Public/AssetManager.h"
#include "Framework/Asset/Public/TextureAsset.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/UI/Public/UIManager.h"
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
        constexpr uint32 PointLightLimit = 4;

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

        bool CreateGBuffer(
            RHIDevice& Device,
            std::unique_ptr<RHITexture>& Texture,
            uint32& Descriptor,
            uint32 Width,
            uint32 Height,
            ERHIFormat Format,
            std::string* ErrorMessage)
        {
            Texture = Device.CreateColorTarget(Width, Height, Format, ErrorMessage);
            if (Texture == nullptr)
            {
                return false;
            }

            const bool bCreated = Descriptor == 0xffffffff
                ? Device.CreateShaderResourceView(*Texture, Descriptor, ErrorMessage)
                : Device.UpdateShaderResourceView(*Texture, Descriptor, ErrorMessage);
            return bCreated;
        }
    }

    DeferredRenderer::DeferredRenderer()
    {
        FramePass = &Pass;
    }

    bool DeferredRenderer::EnsureTargets(std::string* ErrorMessage)
    {
        const uint32 Width = SwapChain->GetWidth();
        const uint32 Height = SwapChain->GetHeight();
        if (Width == 0 || Height == 0)
        {
            SetErrorMessage(ErrorMessage, "Deferred targets require a non-zero viewport");
            return false;
        }

        if (GBufferSampler == nullptr)
        {
            GBufferSampler = Device->CreateClampSampler(ErrorMessage);
            if (GBufferSampler == nullptr)
            {
                return false;
            }
        }

        if (LightConstants == nullptr)
        {
            LightConstants = Device->CreateConstantBuffer(512, ErrorMessage);
            if (LightConstants == nullptr)
            {
                return false;
            }
        }

        if (LightPipeline == nullptr)
        {
            const ShaderHandle Shader = ShaderLibrary->Register(
                "PostProcess/Fullscreen.slang",
                "Light/DeferredLight.slang",
                {},
                {});
            if (!Shader.IsValid() || !ShaderLibrary->LoadShaders(*Device, Shader, ErrorMessage))
            {
                return false;
            }

            const ShaderReflection* VertexReflection = ShaderLibrary->GetReflection(Shader, ERHIShaderStage::Vertex);
            const ShaderReflection* PixelReflection = ShaderLibrary->GetReflection(Shader, ERHIShaderStage::Pixel);
            if (PixelReflection != nullptr)
            {
                for (const ShaderBinding& Binding : PixelReflection->Bindings)
                {
                    if (Binding.Kind == EShaderBindingKind::ConstantBuffer && Binding.Size != 0 && Binding.Size != 448)
                    {
                        SetErrorMessage(
                            ErrorMessage,
                            "Deferred light constant buffer is " + std::to_string(Binding.Size) + " bytes");
                        return false;
                    }
                }
            }
            const RHIPipelineLayout Layout = VertexReflection != nullptr && PixelReflection != nullptr
                ? RHIPipelineLayout::FromReflection(*VertexReflection, *PixelReflection)
                : RHIPipelineLayout{};
            RHIGraphicsPipelineDesc PipelineDesc;
            PipelineDesc.VertexShader = ShaderLibrary->GetVertexShader(Shader);
            PipelineDesc.PixelShader = ShaderLibrary->GetPixelShader(Shader);
            PipelineDesc.Layout = &Layout;
            PipelineDesc.RenderTargetFormats[0] = ERHIFormat::R16G16B16A16_Float;
            PipelineDesc.RenderTargetCount = 1;
            PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
            PipelineDesc.bEnableDepth = false;
            LightPipeline = Device->CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
            if (LightPipeline == nullptr)
            {
                return false;
            }
        }

        if (GBufferA == nullptr || TargetWidth != Width || TargetHeight != Height)
        {
            if (!CreateGBuffer(*Device, GBufferA, GBufferADescriptor, Width, Height, ERHIFormat::R8G8B8A8_UNorm, ErrorMessage) ||
                !CreateGBuffer(*Device, GBufferB, GBufferBDescriptor, Width, Height, ERHIFormat::R16G16B16A16_Float, ErrorMessage) ||
                !CreateGBuffer(*Device, GBufferC, GBufferCDescriptor, Width, Height, ERHIFormat::R8G8B8A8_UNorm, ErrorMessage))
            {
                return false;
            }

            TargetWidth = Width;
            TargetHeight = Height;
        }

        if (DepthResource != DepthBuffer.get() || DepthDescriptor == 0xffffffff)
        {
            const bool bCreated = DepthDescriptor == 0xffffffff
                ? Device->CreateShaderResourceView(*DepthBuffer, DepthDescriptor, ErrorMessage)
                : Device->UpdateShaderResourceView(*DepthBuffer, DepthDescriptor, ErrorMessage);
            if (!bCreated)
            {
                return false;
            }

            DepthResource = DepthBuffer.get();
        }

        return true;
    }

    bool DeferredRenderer::PrepareFrame()
    {
        std::string Error;
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

        if (!Shadows.Ensure(*Device, *ShaderLibrary, &Error))
        {
            Log::Error("{}", Error);
            return false;
        }

        if (!EnsureTargets(&Error))
        {
            Log::Error("{}", Error);
            return false;
        }

        if (UIManager::Get().GetState().Post.bTAA)
        {
            ProjectionJitterX = Halton(FrameIndex, 2) - 0.5F;
            ProjectionJitterY = Halton(FrameIndex, 3) - 0.5F;
            ++FrameIndex;
            if (FrameIndex == 0)
            {
                FrameIndex = 1;
            }
        }
        else
        {
            ProjectionJitterX = 0.0F;
            ProjectionJitterY = 0.0F;
        }

        SelectVisiblePointLights(PointLightLimit, VisiblePointLights);
        const DirectLightRenderProxy* DirectLight = SceneProxy.bHasDirectLight ? &SceneProxy.DirectLight : nullptr;
        if (!Shadows.Prepare(MeshBatches, SceneProxy.Meshes, *Assets, DirectLight, VisiblePointLights, &Error))
        {
            Log::Error("{}", Error);
            return false;
        }

        return true;
    }

    void DeferredRenderer::PopulateContext(RenderPassContext& Context)
    {
        Renderer::PopulateContext(Context);
        Context.PointLights = &VisiblePointLights;
        Context.SkyLight = SceneProxy.bHasSkyLight ? &SceneProxy.SkyLight : nullptr;
        Context.Shadow = &Shadows.GetBinding();
        Context.Shadows = &Shadows;
        Context.ShadowCasters = &SceneProxy.Meshes;
        Context.SkyPipeline = SkyPipeline.get();
        Context.SkyConstants = SkyConstantBuffer.get();
        Context.ImageBasedLight = &Lighting.GetBinding();
        Context.EnvironmentDescriptor = EnvironmentDescriptor;
        Context.EnvironmentSampler = EnvironmentSampler;
        Context.SceneColor = Post.GetSceneColor();
        Context.Post = &Post;
        Context.GBufferA = GBufferA.get();
        Context.GBufferB = GBufferB.get();
        Context.GBufferC = GBufferC.get();
        Context.DeferredLightPipeline = LightPipeline.get();
        Context.DeferredLightConstants = LightConstants.get();
        Context.GBufferADescriptor = GBufferADescriptor;
        Context.GBufferBDescriptor = GBufferBDescriptor;
        Context.GBufferCDescriptor = GBufferCDescriptor;
        Context.GBufferDepthDescriptor = DepthDescriptor;
        Context.GBufferSampler = GBufferSampler == nullptr ? 0xffffffff : GBufferSampler->GetDescriptorIndex();
        Context.ProjectionJitterX = ProjectionJitterX;
        Context.ProjectionJitterY = ProjectionJitterY;
        Context.PreviousViewProjection = bHasPrevious ? PreviousViewProjection : nullptr;
        Context.PreviousWorlds = bHasPrevious ? &PreviousWorlds : nullptr;
        Context.StoredViewProjection = PreviousViewProjection;
    }

    void DeferredRenderer::StoreFrameHistory()
    {
        PreviousWorlds.resize(SceneProxy.Meshes.size() * 16);
        for (size_t Index = 0; Index < SceneProxy.Meshes.size(); ++Index)
        {
            std::memcpy(PreviousWorlds.data() + Index * 16, SceneProxy.Meshes[Index].World, sizeof(float) * 16);
        }
        bHasPrevious = true;
    }

    const char* DeferredRenderer::GetName() const
    {
        return "DeferredRenderer";
    }
}
