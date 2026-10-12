#include "Framework/Render/Public/BlinnPhongRenderer.h"

namespace ShadowEngine
{
    bool BlinnPhongRenderer::RenderFrame()
    {
        if (Assets == nullptr || ShaderLibrary == nullptr)
        {
            return false;
        }

        if (!CollectVisibleItems(ItemFilter, DrawQueue))
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
        Context.DirectLight = SceneProxy.bHasDirectLight ? &SceneProxy.DirectLight : nullptr;
        Context.ClearColor = ClearColor;
        Context.ConstantCapacity = ConstantCapacity;
        Context.ConstantAlignment = ConstantAlignment;
        return Pass.Execute(DrawQueue, Context);
    }

    const char* BlinnPhongRenderer::GetName() const
    {
        return "BlinnPhongRenderer";
    }
}
