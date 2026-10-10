#include "Framework/Render/Public/ForwardRenderPass.h"

#include "Framework/Render/Public/SkyBackground.h"

#include "Framework/Asset/Public/AssetManager.h"
#include "Framework/Asset/Public/MeshAsset.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/Material/Public/Material.h"
#include "Framework/Material/Public/MaterialInstance.h"
#include "Framework/Material/Public/MaterialParameter.h"
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
            MaterialParameter::SurfaceConstants Surface;
            uint32 BaseColorDescriptor = 0;
            uint32 RoughnessDescriptor = 0;
            uint32 NormalDescriptor = 0;
            uint32 SamplerDescriptor = 0;
            float DirectDirection[4];
            float DirectColor[4];
            float PointPosition[4];
            float PointColor[4];
            float SkyColor[4];
        };
    }

    bool ForwardRenderPass::Execute(const RenderQueue& Queue, const RenderPassContext& Context)
    {
        static_assert(sizeof(GpuObjectConstants) <= 512);
        if (Context.Device == nullptr || Context.SwapChain == nullptr || Context.CommandList == nullptr ||
            Context.ConstantBuffer == nullptr || Context.DepthBuffer == nullptr || Context.Assets == nullptr ||
            Context.Shaders == nullptr || Context.View == nullptr || Context.Batches == nullptr ||
            Context.TextureDescriptors == nullptr || Context.Samplers == nullptr || Context.Lights == nullptr ||
            Context.PointLights == nullptr || Context.SkyLights == nullptr)
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
        const glm::vec3 CameraPosition = Context.View->GetPosition();

        DirectLightRenderProxy Direct{};
        Direct.Intensity = 0.0F;
        if (!Context.Lights->empty())
        {
            Direct = Context.Lights->front();
        }

        PointLightRenderProxy Point{};
        Point.Intensity = 0.0F;
        if (!Context.PointLights->empty())
        {
            Point = Context.PointLights->front();
        }

        SkyLightRenderProxy Sky{};
        Sky.Intensity = 0.0F;
        if (!Context.SkyLights->empty())
        {
            Sky = Context.SkyLights->front();
        }

        std::vector<uint8> Constants(Items.size() * Context.ConstantAlignment);
        for (size_t Index = 0; Index < Items.size(); ++Index)
        {
            const RenderItem& Item = Items[Index];
            const MeshAsset* Mesh = Context.Assets->ResolveMesh(Item.Mesh);
            const MaterialInstance* Instance = Context.Assets->ResolveMaterialInstance(Item.Material);
            const Material* BoundMaterial = Instance == nullptr ? nullptr : Context.Assets->ResolveMaterial(Instance->GetBaseMaterial());
            const ShaderHandle* Shader = BoundMaterial == nullptr ? nullptr : BoundMaterial->FindShader("PBRForwardRenderer");
            if (Mesh == nullptr || Instance == nullptr || Shader == nullptr ||
                Context.Shaders->GetVertexShader(*Shader) == nullptr ||
                Context.Shaders->GetPixelShader(*Shader) == nullptr ||
                FindMeshBatch(*Context.Batches, Item.Mesh, Item.Section) == nullptr)
            {
                Log::Error("Render item is missing a mesh, material, or shader");
                return false;
            }

            const MaterialParameterBlock& Parameter = Instance->GetParameters();
            GpuObjectConstants Slot{};
            std::memcpy(Slot.ViewProjection, glm::value_ptr(ViewProjection), sizeof(Slot.ViewProjection));
            std::memcpy(Slot.World, Item.World, sizeof(Slot.World));
            std::memcpy(Slot.NormalMatrix, Item.NormalMatrix, sizeof(Slot.NormalMatrix));
            Slot.CameraPosition[0] = CameraPosition.x;
            Slot.CameraPosition[1] = CameraPosition.y;
            Slot.CameraPosition[2] = CameraPosition.z;
            Slot.LightDirection[0] = Direct.Direction[0];
            Slot.LightDirection[1] = Direct.Direction[1];
            Slot.LightDirection[2] = Direct.Direction[2];
            Slot.LightColor[0] = Direct.Color[0];
            Slot.LightColor[1] = Direct.Color[1];
            Slot.LightColor[2] = Direct.Color[2];
            Slot.LightColor[3] = Direct.Intensity;
            Slot.DirectDirection[0] = Direct.Direction[0];
            Slot.DirectDirection[1] = Direct.Direction[1];
            Slot.DirectDirection[2] = Direct.Direction[2];
            Slot.DirectColor[0] = Direct.Color[0];
            Slot.DirectColor[1] = Direct.Color[1];
            Slot.DirectColor[2] = Direct.Color[2];
            Slot.DirectColor[3] = Direct.Intensity;
            Slot.PointPosition[0] = Point.Position[0];
            Slot.PointPosition[1] = Point.Position[1];
            Slot.PointPosition[2] = Point.Position[2];
            Slot.PointPosition[3] = Point.Radius;
            Slot.PointColor[0] = Point.Color[0];
            Slot.PointColor[1] = Point.Color[1];
            Slot.PointColor[2] = Point.Color[2];
            Slot.PointColor[3] = Point.Intensity;
            Slot.SkyColor[0] = Sky.Color[0];
            Slot.SkyColor[1] = Sky.Color[1];
            Slot.SkyColor[2] = Sky.Color[2];
            Slot.SkyColor[3] = Sky.Intensity;
            MaterialParameter::WriteSurfaceConstants(Parameter, Slot.Surface);
            const MaterialParameter::TextureBinding Textures = MaterialParameter::GetTextureBinding(Parameter);
            if (Textures.HasTextures())
            {
                const auto BaseColor = Context.TextureDescriptors->find(Textures.BaseColor.Index);
                const auto Roughness = Context.TextureDescriptors->find(Textures.Roughness.Index);
                const auto Normal = Context.TextureDescriptors->find(Textures.Normal.Index);
                const auto Sampler = Context.Samplers->find(Textures.Sampler.Index);
                if (BaseColor == Context.TextureDescriptors->end() || Roughness == Context.TextureDescriptors->end() ||
                    Normal == Context.TextureDescriptors->end() || Sampler == Context.Samplers->end() || Sampler->second == nullptr)
                {
                    Log::Error("Material descriptor is missing");
                    return false;
                }

                Slot.BaseColorDescriptor = BaseColor->second;
                Slot.RoughnessDescriptor = Roughness->second;
                Slot.NormalDescriptor = Normal->second;
                Slot.SamplerDescriptor = Sampler->second->GetDescriptorIndex();
            }
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
