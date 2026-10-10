#include "Framework/Render/Public/PBRForwardRenderer.h"

namespace ShadowEngine
{
    bool PBRForwardRenderer::RenderFrame()
    {
        if (Assets == nullptr || ShaderLibrary == nullptr)
        {
            return false;
        }

        if (!FillRenderQueue(SceneProxy, ItemFilter, *Assets, GetName(), MeshBatches, DrawQueue))
        {
            return false;
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
        Context.Lights = &SceneProxy.DirectLights;
        Context.PointLights = &SceneProxy.PointLights;
        Context.SkyLights = &SceneProxy.SkyLights;
        Context.SkyPipeline = SkyPipeline.get();
        Context.SkyConstants = SkyConstantBuffer.get();
        Context.EnvironmentDescriptor = EnvironmentDescriptor;
        Context.EnvironmentSampler = EnvironmentSampler;
        Context.ClearColor = ClearColor;
        Context.ConstantCapacity = ConstantCapacity;
        Context.ConstantAlignment = ConstantAlignment;
        return Pass.Execute(DrawQueue, Context);
    }

    const char* PBRForwardRenderer::GetName() const
    {
        return "PBRForwardRenderer";
    }
}
