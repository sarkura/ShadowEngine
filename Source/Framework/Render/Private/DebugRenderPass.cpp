#include "Framework/Render/Public/DebugRenderPass.h"

#include "Framework/Render/Public/SkyBackground.h"

#include "Framework/Asset/Public/AssetManager.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/Material/Public/Material.h"
#include "Framework/Material/Public/MaterialInstance.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/RHI/Public/RHISwapChain.h"
#include "Framework/RHI/Public/RHITexture.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <cstring>
#include <vector>

namespace ShadowEngine
{
    namespace
    {
        struct GpuObjectConstants
        {
            float ViewProjection[16];
            float World[16];
            float NormalMatrix[16];
            float CameraPosition[4];
            float LightDirection[4];
            float LightColor[4];
        };
    }

    bool DebugRenderPass::Execute(const RenderQueue& Queue, const RenderPassContext& Context)
    {
        static_assert(sizeof(GpuObjectConstants) <= 512);
        if (Context.Device == nullptr || Context.SwapChain == nullptr || Context.CommandList == nullptr ||
            Context.ConstantBuffer == nullptr || Context.DepthBuffer == nullptr || Context.Assets == nullptr ||
            Context.Shaders == nullptr || Context.View == nullptr || Context.Batches == nullptr)
        {
            return false;
        }

        const std::vector<RenderItem>& Items = Queue.GetItems();
        if (Items.size() > Context.ConstantCapacity)
        {
            Log::Error("Scene has more objects than the constant buffer can hold");
            return false;
        }

        const float Width = static_cast<float>(Context.SwapChain->GetWidth());
        const float Height = static_cast<float>(Context.SwapChain->GetHeight());
        const float Aspect = Height > 0.0F ? Width / Height : 1.0F;
        const glm::mat4 View = Context.View->ViewMatrix();
        const glm::mat4 Projection = glm::perspectiveRH_ZO(glm::radians(45.0F), Aspect, 0.1F, 100.0F);
        const glm::mat4 ViewProjection = Projection * View;

        std::vector<uint8> Constants(Items.size() * Context.ConstantAlignment);
        for (size_t Index = 0; Index < Items.size(); ++Index)
        {
            const RenderItem& Item = Items[Index];
            const MaterialInstance* Instance = Context.Assets->ResolveMaterialInstance(Item.Material);
            const Material* BoundMaterial = Instance == nullptr ? nullptr : Context.Assets->ResolveMaterial(Instance->GetBaseMaterial());
            const ShaderHandle* Shader = BoundMaterial == nullptr ? nullptr : BoundMaterial->FindShader("DebugRenderer");
            if (Context.Assets->ResolveMesh(Item.Mesh) == nullptr || Instance == nullptr || Shader == nullptr ||
                Context.Shaders->GetVertexShader(*Shader) == nullptr ||
                Context.Shaders->GetPixelShader(*Shader) == nullptr ||
                FindMeshBatch(*Context.Batches, Item.Mesh, Item.Section) == nullptr)
            {
                Log::Error("Render item is missing a mesh, material, or shader");
                return false;
            }

            GpuObjectConstants Slot{};
            std::memcpy(Slot.ViewProjection, glm::value_ptr(ViewProjection), sizeof(Slot.ViewProjection));
            std::memcpy(Slot.World, Item.World, sizeof(Slot.World));
            std::memcpy(Slot.NormalMatrix, Item.NormalMatrix, sizeof(Slot.NormalMatrix));
            std::memcpy(Constants.data() + Index * Context.ConstantAlignment, &Slot, sizeof(Slot));
        }
        if (!Constants.empty() && !Context.ConstantBuffer->Update(0, Constants))
        {
            Log::Error("Failed to write scene constants");
            return false;
        }

        Context.CommandList->Begin();
        Context.CommandList->BeginRenderPass(Context.SwapChain->GetCurrentBackBuffer(), Context.DepthBuffer, Context.ClearColor);
        Context.CommandList->SetViewport({0.0F, 0.0F, Width, Height, 0.0F, 1.0F});
        Context.CommandList->SetScissor({
            0,
            0,
            static_cast<int32>(Context.SwapChain->GetWidth()),
            static_cast<int32>(Context.SwapChain->GetHeight())});
        if (!DrawSkyBackground(Context))
        {
            return false;
        }
        for (uint32 Index = 0; Index < static_cast<uint32>(Items.size()); ++Index)
        {
            const MeshBatch* Batch = FindMeshBatch(*Context.Batches, Items[Index].Mesh, Items[Index].Section);
            Context.CommandList->SetPipeline(*Batch->Pipeline);
            Context.CommandList->BindShaderResources();
            Context.CommandList->SetVertexBuffer(*Batch->VertexBuffer);
            Context.CommandList->SetIndexBuffer(*Batch->IndexBuffer);
            Context.CommandList->SetConstantBuffer(*Context.ConstantBuffer, Index * Context.ConstantAlignment);
            Context.CommandList->DrawIndexed(Batch->IndexCount);
        }
        Context.CommandList->EndRenderPass();
        Context.CommandList->End();

        Context.Device->SubmitCommandList(*Context.CommandList);
        const bool bPresented = Context.SwapChain->Present();
        Context.Device->WaitIdle();
        return bPresented;
    }
}
