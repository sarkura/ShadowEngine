#include "Framework/Render/Public/PostProcess.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/UI/Public/UIManager.h"
#include "Framework/Render/Public/MeshBatch.h"
#include "Framework/Render/Public/RenderGraph.h"
#include "Framework/Render/Public/RenderItem.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIDefinitions.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "Framework/RHI/Public/RHIPipelineLayout.h"
#include "Framework/RHI/Public/RHISampler.h"
#include "Framework/RHI/Public/RHITexture.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <algorithm>
#include <cstring>
#include <span>

namespace ShadowEngine
{
    bool PostProcess::Ensure(
        RHIDevice& Device,
        ShaderManager& Shaders,
        RHITexture& Depth,
        uint32 InWidth,
        uint32 InHeight,
        ERHIFormat OutputFormat,
        std::string* ErrorMessage)
    {
        static_assert(sizeof(PostGpuConstants) == 240);
        if (InWidth == 0 || InHeight == 0)
        {
            SetErrorMessage(ErrorMessage, "Post process requires a non-zero viewport");
            return false;
        }

        if (!bPipelinesReady)
        {
            if (!CreateComputePipeline(Device, Shaders, "PostProcess/CameraVelocity.slang", CameraVelocityPipeline, ErrorMessage) ||
                !CreateComputePipeline(Device, Shaders, "PostProcess/TAA.slang", TAAPipeline, ErrorMessage) ||
                !CreateComputePipeline(Device, Shaders, "PostProcess/BloomExtract.slang", BloomExtractPipeline, ErrorMessage) ||
                !CreateComputePipeline(Device, Shaders, "PostProcess/BloomDownsample.slang", BloomDownsamplePipeline, ErrorMessage) ||
                !CreateComputePipeline(Device, Shaders, "PostProcess/BloomUpsample.slang", BloomUpsamplePipeline, ErrorMessage) ||
                !CreateComputePipeline(Device, Shaders, "PostProcess/ExposureLuminance.slang", ExposureLuminancePipeline, ErrorMessage) ||
                !CreateComputePipeline(Device, Shaders, "PostProcess/ExposureReduce.slang", ExposureReducePipeline, ErrorMessage) ||
                !CreateComputePipeline(Device, Shaders, "PostProcess/ExposureAdapt.slang", ExposureAdaptPipeline, ErrorMessage) ||
                !CreateComputePipeline(Device, Shaders, "PostProcess/Tonemap.slang", TonemapPipeline, ErrorMessage))
            {
                return false;
            }

            const ShaderHandle VelocityShader = Shaders.Register(
                "PostProcess/MeshVelocity.slang",
                "PostProcess/MeshVelocity.slang",
                {},
                {});
            if (!VelocityShader.IsValid() || !Shaders.LoadShaders(Device, VelocityShader, ErrorMessage))
            {
                return false;
            }

            const ShaderReflection* VertexReflection = Shaders.GetReflection(VelocityShader, ERHIShaderStage::Vertex);
            const ShaderReflection* PixelReflection = Shaders.GetReflection(VelocityShader, ERHIShaderStage::Pixel);
            const RHIPipelineLayout Layout = VertexReflection != nullptr && PixelReflection != nullptr
                ? RHIPipelineLayout::FromReflection(*VertexReflection, *PixelReflection)
                : RHIPipelineLayout{};
            constexpr RHIInputElement InputLayout[] = {
                {"POSITION", 0, 0, ERHIVertexFormat::Float32x3},
            };
            RHIShader* MeshShader = Shaders.LoadStandalone(
                Device,
                "Mesh/VelocityMesh.slang",
                "MeshMain",
                ERHIShaderStage::Mesh,
                ErrorMessage);
            if (MeshShader == nullptr)
            {
                return false;
            }

            RHIGraphicsPipelineDesc PipelineDesc;
            PipelineDesc.MeshShader = MeshShader;
            PipelineDesc.PixelShader = Shaders.GetPixelShader(VelocityShader);
            PipelineDesc.Layout = &Layout;
            PipelineDesc.RenderTargetFormats[0] = ERHIFormat::R16G16B16A16_Float;
            PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
            PipelineDesc.InputLayout = InputLayout;
            PipelineDesc.bEnableDepth = true;
            PipelineDesc.bWriteDepth = false;
            PipelineDesc.bDepthEqual = true;
            PipelineDesc.DepthFormat = ERHIFormat::D32_Float;
            PipelineDesc.bFrontCounterClockwise = true;
            PipelineDesc.Cull = ERHICullMode::Back;
            MeshVelocityPipeline = Device.CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
            PipelineDesc.Cull = ERHICullMode::None;
            MeshVelocityTwoSidedPipeline = Device.CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
            if (MeshVelocityPipeline == nullptr || MeshVelocityTwoSidedPipeline == nullptr)
            {
                return false;
            }

            ClampSampler = Device.CreateClampSampler(ErrorMessage);
            ConstantBuffer = Device.CreateConstantBuffer(256 * 16, ErrorMessage);
            if (ClampSampler == nullptr || ConstantBuffer == nullptr)
            {
                return false;
            }

            bPipelinesReady = true;
        }

        if (ExposureLum.Texture == nullptr)
        {
            if (!CreateTarget(Device, ExposureLum, 16, 16, ErrorMessage, true) ||
                !CreateTarget(Device, ExposureAverage, 1, 1, ErrorMessage, true) ||
                !CreateTarget(Device, Exposure[0], 1, 1, ErrorMessage, true) ||
                !CreateTarget(Device, Exposure[1], 1, 1, ErrorMessage, true))
            {
                return false;
            }
        }

        const bool bResize = Width != InWidth || Height != InHeight || SceneColor.Texture == nullptr;
        if (bResize)
        {
            if (!CreateTarget(Device, SceneColor, InWidth, InHeight, ErrorMessage) ||
                !CreateTarget(Device, TonemapColor, InWidth, InHeight, ErrorMessage, true, OutputFormat) ||
                !CreateTarget(Device, Motion, InWidth, InHeight, ErrorMessage, true) ||
                !CreateTarget(Device, History[0], InWidth, InHeight, ErrorMessage, true) ||
                !CreateTarget(Device, History[1], InWidth, InHeight, ErrorMessage, true))
            {
                return false;
            }

            uint32 LevelWidth = std::max(1u, InWidth / 2);
            uint32 LevelHeight = std::max(1u, InHeight / 2);
            for (Target& Level : BloomDown)
            {
                if (!CreateTarget(Device, Level, LevelWidth, LevelHeight, ErrorMessage, true))
                {
                    return false;
                }

                LevelWidth = std::max(1u, LevelWidth / 2);
                LevelHeight = std::max(1u, LevelHeight / 2);
            }

            LevelWidth = std::max(1u, InWidth / 2);
            LevelHeight = std::max(1u, InHeight / 2);
            for (Target& Level : BloomUp)
            {
                if (!CreateTarget(Device, Level, LevelWidth, LevelHeight, ErrorMessage, true))
                {
                    return false;
                }

                LevelWidth = std::max(1u, LevelWidth / 2);
                LevelHeight = std::max(1u, LevelHeight / 2);
            }

            Width = InWidth;
            Height = InHeight;
            bHistoryValid = false;
            bExposureValid = false;
            HistoryIndex = 0;
            ExposureIndex = 0;
        }

        if (DepthTexture != &Depth || DepthDescriptor == 0xffffffff)
        {
            if (DepthDescriptor == 0xffffffff)
            {
                if (!Device.CreateShaderResourceView(Depth, DepthDescriptor, ErrorMessage))
                {
                    return false;
                }
            }
            else if (!Device.UpdateShaderResourceView(Depth, DepthDescriptor, ErrorMessage))
            {
                return false;
            }

            DepthTexture = &Depth;
        }

        return SceneColor.Texture != nullptr;
    }

    RHITexture* PostProcess::GetSceneColor() const
    {
        return SceneColor.Texture.get();
    }

    bool PostProcess::CreateTarget(
        RHIDevice& Device,
        Target& Out,
        uint32 InWidth,
        uint32 InHeight,
        std::string* ErrorMessage,
        bool bUnorderedAccess,
        ERHIFormat Format)
    {
        Out.Texture = Device.CreateColorTarget(
            InWidth,
            InHeight,
            Format,
            ErrorMessage,
            bUnorderedAccess);
        if (Out.Texture == nullptr)
        {
            return false;
        }

        const bool bCreated = Out.Descriptor == 0xffffffff
            ? Device.CreateShaderResourceView(*Out.Texture, Out.Descriptor, ErrorMessage)
            : Device.UpdateShaderResourceView(*Out.Texture, Out.Descriptor, ErrorMessage);
        if (!bCreated)
        {
            return false;
        }

        if (bUnorderedAccess)
        {
            const bool bView = Out.UnorderedAccess == 0xffffffff
                ? Device.CreateUnorderedAccessView(*Out.Texture, Out.UnorderedAccess, ErrorMessage)
                : Device.UpdateUnorderedAccessView(*Out.Texture, Out.UnorderedAccess, ErrorMessage);
            if (!bView)
            {
                return false;
            }
        }

        Out.Width = InWidth;
        Out.Height = InHeight;
        return true;
    }

    bool PostProcess::CreateComputePipeline(
        RHIDevice& Device,
        ShaderManager& Shaders,
        const char* ComputePath,
        std::unique_ptr<RHIPipeline>& Out,
        std::string* ErrorMessage)
    {
        RHIShader* ComputeShader = Shaders.LoadStandalone(
            Device,
            ComputePath,
            "ComputeMain",
            ERHIShaderStage::Compute,
            ErrorMessage);
        if (ComputeShader == nullptr)
        {
            return false;
        }

        Out = Device.CreateComputePipeline(*ComputeShader, ErrorMessage);
        return Out != nullptr;
    }

    bool PostProcess::DrawCompute(
        RHICommandList& Commands,
        Target& TargetTexture,
        RHIPipeline& Pipeline,
        const PostGpuConstants& Constants)
    {
        static_assert(sizeof(PostGpuConstants) == 240);
        static_assert(sizeof(PostGpuConstants) <= 256);
        if (TargetTexture.Texture == nullptr || TargetTexture.UnorderedAccess == 0xffffffff || TargetTexture.Width == 0 ||
            TargetTexture.Height == 0)
        {
            Log::Error("Compute post target is missing an unordered access view");
            return false;
        }

        PostGpuConstants Written = Constants;
        Written.Output = TargetTexture.UnorderedAccess;
        const std::span<const uint8> Bytes(reinterpret_cast<const uint8*>(&Written), sizeof(Written));
        if (ConstantBuffer == nullptr || ConstantSlot >= 16 || !ConstantBuffer->Update(ConstantSlot * 256, Bytes))
        {
            Log::Error("Failed to write post constants");
            return false;
        }

        const uint32 Offset = ConstantSlot * 256;
        ++ConstantSlot;
        Commands.UseAsUnorderedAccess(*TargetTexture.Texture);
        Commands.SetPipeline(Pipeline);
        Commands.BindShaderResources();
        Commands.SetConstantBuffer(*ConstantBuffer, Offset);
        Commands.Dispatch((TargetTexture.Width + 7u) / 8u, (TargetTexture.Height + 7u) / 8u, 1);
        Commands.UseAsShaderResource(*TargetTexture.Texture);
        return true;
    }

    bool PostProcess::Draw(
        RHICommandList& Commands,
        RHITexture& BackBuffer,
        RHITexture& Depth,
        RHIBuffer& ObjectConstants,
        const std::vector<RenderItem>& Items,
        const std::vector<MeshBatch>& Batches,
        const float InverseViewProjection[16],
        const float PreviousViewProjection[16],
        const float CameraPosition[3],
        uint32 InWidth,
        uint32 InHeight,
        uint32 ConstantAlignment,
        float DeltaTime)
    {
        if (SceneColor.Texture == nullptr || Motion.Texture == nullptr || CameraVelocityPipeline == nullptr ||
            MeshVelocityPipeline == nullptr || MeshVelocityTwoSidedPipeline == nullptr || TAAPipeline == nullptr ||
            BloomExtractPipeline == nullptr ||
            BloomDownsamplePipeline == nullptr || BloomUpsamplePipeline == nullptr ||
            ExposureLuminancePipeline == nullptr || ExposureReducePipeline == nullptr || ExposureAdaptPipeline == nullptr ||
            TonemapPipeline == nullptr || TonemapColor.Texture == nullptr || ClampSampler == nullptr ||
            ExposureLum.Texture == nullptr)
        {
            Log::Error("Forward post process is not ready");
            return false;
        }

        const UIManager::State Interface = UIManager::Get().GetState();
        if (!Interface.Post.bTAA)
        {
            bHistoryValid = false;
        }
        if (!Interface.Post.bAutoExposure)
        {
            bExposureValid = false;
        }

        Commands.UseAsShaderResource(*SceneColor.Texture);
        Commands.UseAsShaderResource(Depth);
        ConstantSlot = 0;

        PostGpuConstants Constants{};
        std::memcpy(Constants.InverseViewProjection, InverseViewProjection, sizeof(Constants.InverseViewProjection));
        std::memcpy(Constants.PreviousViewProjection, PreviousViewProjection, sizeof(Constants.PreviousViewProjection));
        Constants.CameraPosition[0] = CameraPosition[0];
        Constants.CameraPosition[1] = CameraPosition[1];
        Constants.CameraPosition[2] = CameraPosition[2];
        Constants.Sampler = ClampSampler->GetDescriptorIndex();
        Constants.Depth = DepthDescriptor;
        Constants.Motion = Motion.Descriptor;
        Constants.ExposureMiddleGrey = Interface.Post.ExposureMiddleGrey;
        Constants.ExposureMinimum = Interface.Post.ExposureMinimum;
        Constants.ExposureMaximum = Interface.Post.ExposureMaximum;
        Constants.ExposureAdaptSpeed = Interface.Post.ExposureAdaptSpeed;
        Constants.BloomLimit = Interface.Post.BloomLimit;
        if (Interface.Post.bAutoExposure)
        {
            Constants.PostFlags |= 1u;
        }
        if (Interface.Post.bBloom)
        {
            Constants.PostFlags |= 2u;
        }
        if (Interface.Post.bTonemapDither)
        {
            Constants.PostFlags |= 4u;
        }

        uint32 ResolvedColor = SceneColor.Descriptor;
        RenderGraph Graph;
        const RenderGraphResource SceneInput = Graph.ImportResource("Post.SceneColor");
        const RenderGraphResource DepthInput = Graph.ImportResource("Post.Depth");
        RenderGraphResource ResolvedInput = SceneInput;

        if (Interface.Post.bTAA)
        {
            const RenderGraphResource CameraMotion = Graph.CreateResource("Post.CameraMotion");
            const RenderGraphResource MeshMotion = Graph.CreateResource("Post.MeshMotion");
            const RenderGraphResource TAAOutput = Graph.CreateResource("Post.TAAOutput");
            const uint32 WriteIndex = HistoryIndex ^ 1u;
            Graph.AddPass("Post.CameraVelocity", {DepthInput}, {CameraMotion}, [&]()
            {
                Constants.DestSize[0] = static_cast<float>(InWidth);
                Constants.DestSize[1] = static_cast<float>(InHeight);
                Constants.Source = SceneColor.Descriptor;
                return DrawCompute(Commands, Motion, *CameraVelocityPipeline, Constants);
            });
            Graph.AddPass("Post.MeshVelocity", {CameraMotion, DepthInput}, {MeshMotion}, [&]()
            {
                Commands.BeginRenderPass(*Motion.Texture, &Depth, {}, false, false);
                Commands.SetViewport({0.0F, 0.0F, static_cast<float>(InWidth), static_cast<float>(InHeight), 0.0F, 1.0F});
                Commands.SetScissor({0, 0, static_cast<int32>(InWidth), static_cast<int32>(InHeight)});
                Commands.BindShaderResources();
                for (uint32 Index = 0; Index < static_cast<uint32>(Items.size()); ++Index)
                {
                    const MeshBatch* Batch = FindMeshBatch(Batches, Items[Index].Mesh, Items[Index].Section);
                    if (Batch == nullptr || Batch->VertexBuffer == nullptr || Batch->Meshlets == nullptr)
                    {
                        Log::Error("Forward velocity is missing a mesh");
                        Commands.EndRenderPass();
                        return false;
                    }

                    DrawMeshBatch(
                        Commands,
                        *Batch,
                        *(Batch->bTwoSided ? MeshVelocityTwoSidedPipeline : MeshVelocityPipeline),
                        ObjectConstants,
                        Index * ConstantAlignment);
                }
                Commands.EndRenderPass();
                Commands.UseAsShaderResource(*Motion.Texture);
                return true;
            });
            Graph.AddPass("Post.TAAResolve", {SceneInput, MeshMotion}, {TAAOutput}, [&]()
            {
                Constants.Source = SceneColor.Descriptor;
                Constants.Secondary = History[HistoryIndex].Descriptor;
                Constants.CurrentWeight = bHistoryValid ? Interface.Post.TAACurrentWeight : 1.0F;
                if (!DrawCompute(Commands, History[WriteIndex], *TAAPipeline, Constants))
                {
                    return false;
                }
                Commands.UseAsShaderResource(*History[WriteIndex].Texture);
                ResolvedColor = History[WriteIndex].Descriptor;
                HistoryIndex = WriteIndex;
                bHistoryValid = true;
                return true;
            });
            ResolvedInput = TAAOutput;
        }

        RenderGraphResource ExposureOutput = Graph.ImportResource("Post.ExposureDisabled");
        if (Interface.Post.bAutoExposure)
        {
            const RenderGraphResource LuminanceOutput = Graph.CreateResource("Post.ExposureLuminance");
            const RenderGraphResource AverageOutput = Graph.CreateResource("Post.ExposureAverage");
            ExposureOutput = Graph.CreateResource("Post.ExposureOutput");
            const uint32 ExposureWrite = ExposureIndex ^ 1u;
            Graph.AddPass("Post.ExposureLuminance", {ResolvedInput}, {LuminanceOutput}, [&]()
            {
                Constants.Source = ResolvedColor;
                Constants.DestSize[0] = static_cast<float>(ExposureLum.Width);
                Constants.DestSize[1] = static_cast<float>(ExposureLum.Height);
                if (!DrawCompute(Commands, ExposureLum, *ExposureLuminancePipeline, Constants))
                {
                    return false;
                }
                Commands.UseAsShaderResource(*ExposureLum.Texture);
                return true;
            });
            Graph.AddPass("Post.ExposureReduce", {LuminanceOutput}, {AverageOutput}, [&]()
            {
                Constants.Source = ExposureLum.Descriptor;
                Constants.DestSize[0] = 1.0F;
                Constants.DestSize[1] = 1.0F;
                if (!DrawCompute(Commands, ExposureAverage, *ExposureReducePipeline, Constants))
                {
                    return false;
                }
                Commands.UseAsShaderResource(*ExposureAverage.Texture);
                return true;
            });
            Graph.AddPass("Post.ExposureAdapt", {AverageOutput}, {ExposureOutput}, [&]()
            {
                Commands.UseAsShaderResource(*Exposure[ExposureIndex].Texture);
                Constants.Source = ExposureAverage.Descriptor;
                Constants.Secondary = Exposure[ExposureIndex].Descriptor;
                Constants.CurrentWeight = bExposureValid ? 0.0F : 1.0F;
                Constants.ExposureDelta = DeltaTime;
                if (!DrawCompute(Commands, Exposure[ExposureWrite], *ExposureAdaptPipeline, Constants))
                {
                    return false;
                }
                Commands.UseAsShaderResource(*Exposure[ExposureWrite].Texture);
                Constants.ExposureMap = Exposure[ExposureWrite].Descriptor;
                ExposureIndex = ExposureWrite;
                bExposureValid = true;
                return true;
            });
        }

        RenderGraphResource BloomOutput = Graph.ImportResource("Post.BloomDisabled");
        if (Interface.Post.bBloom)
        {
            RenderGraphResource DownOutputs[4];
            RenderGraphResource UpOutputs[3];
            for (uint32 Level = 0; Level < 4; ++Level)
            {
                DownOutputs[Level] = Graph.CreateResource("Post.BloomDown" + std::to_string(Level));
            }
            for (uint32 Level = 0; Level < 3; ++Level)
            {
                UpOutputs[Level] = Graph.CreateResource("Post.BloomUp" + std::to_string(Level));
            }

            Graph.AddPass("Post.BloomExtract", {ResolvedInput}, {DownOutputs[0]}, [&]()
            {
                Constants.Source = ResolvedColor;
                Constants.Threshold = Interface.Post.BloomThreshold;
                Constants.DestSize[0] = static_cast<float>(BloomDown[0].Width);
                Constants.DestSize[1] = static_cast<float>(BloomDown[0].Height);
                Constants.SourceTexel[0] = 1.0F / static_cast<float>(InWidth);
                Constants.SourceTexel[1] = 1.0F / static_cast<float>(InHeight);
                if (!DrawCompute(Commands, BloomDown[0], *BloomExtractPipeline, Constants))
                {
                    return false;
                }
                Commands.UseAsShaderResource(*BloomDown[0].Texture);
                return true;
            });

            for (uint32 Level = 0; Level < 3; ++Level)
            {
                Graph.AddPass(
                    "Post.BloomDownsample" + std::to_string(Level + 1),
                    {DownOutputs[Level]},
                    {DownOutputs[Level + 1]},
                    [&, Level]()
                {
                    Constants.Source = BloomDown[Level].Descriptor;
                    Constants.DestSize[0] = static_cast<float>(BloomDown[Level + 1].Width);
                    Constants.DestSize[1] = static_cast<float>(BloomDown[Level + 1].Height);
                    Constants.SourceTexel[0] = 1.0F / static_cast<float>(BloomDown[Level].Width);
                    Constants.SourceTexel[1] = 1.0F / static_cast<float>(BloomDown[Level].Height);
                    if (!DrawCompute(Commands, BloomDown[Level + 1], *BloomDownsamplePipeline, Constants))
                    {
                        return false;
                    }
                    Commands.UseAsShaderResource(*BloomDown[Level + 1].Texture);
                    return true;
                });
            }

            for (int32 Level = 2; Level >= 0; --Level)
            {
                const RenderGraphResource Source = Level == 2 ? DownOutputs[3] : UpOutputs[Level + 1];
                const RenderGraphResource Reads[] = {Source, DownOutputs[Level]};
                const RenderGraphResource Writes[] = {UpOutputs[Level]};
                Graph.AddPass(
                    "Post.BloomUpsample" + std::to_string(Level),
                    Reads,
                    Writes,
                    [&, Level]()
                {
                    Constants.Source = Level == 2 ? BloomDown[3].Descriptor : BloomUp[Level + 1].Descriptor;
                    Constants.Secondary = BloomDown[Level].Descriptor;
                    Constants.DestSize[0] = static_cast<float>(BloomUp[Level].Width);
                    Constants.DestSize[1] = static_cast<float>(BloomUp[Level].Height);
                    if (!DrawCompute(Commands, BloomUp[Level], *BloomUpsamplePipeline, Constants))
                    {
                        return false;
                    }
                    Commands.UseAsShaderResource(*BloomUp[Level].Texture);
                    if (Level == 0)
                    {
                        Constants.Secondary = BloomUp[0].Descriptor;
                        Constants.Strength = Interface.Post.BloomStrength;
                    }
                    return true;
                });
            }
            BloomOutput = UpOutputs[0];
        }

        const RenderGraphResource BackBufferOutput = Graph.CreateResource("Post.BackBuffer");
        std::vector<RenderGraphResource> TonemapReads{ResolvedInput, ExposureOutput, BloomOutput};
        const RenderGraphResource TonemapWrites[] = {BackBufferOutput};
        Graph.AddPass("Post.Tonemap", TonemapReads, TonemapWrites, [&]()
        {
            Constants.Source = ResolvedColor;
            Constants.DestSize[0] = static_cast<float>(InWidth);
            Constants.DestSize[1] = static_cast<float>(InHeight);
            if (!DrawCompute(Commands, TonemapColor, *TonemapPipeline, Constants))
            {
                return false;
            }
            Commands.CopyTexture(*TonemapColor.Texture, BackBuffer);
            return true;
        });

        std::string GraphError;
        if (!Graph.Execute(&GraphError))
        {
            Log::Error("{}", GraphError);
            return false;
        }
        return true;
    }
}
