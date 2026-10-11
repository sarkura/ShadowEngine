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
#include "Framework/RHI/Public/RHIPipelineLayout.h"
#include "Framework/RHI/Public/RHITexture.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
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

    bool ForwardRenderPass::Execute(const RenderQueue& Queue, const RenderPassContext& Context)
    {
        static_assert(sizeof(GpuObjectConstants) == 240);
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
            const ShaderHandle* Shader = BoundMaterial == nullptr ? nullptr : BoundMaterial->FindShader("ForwardRenderer");
            if (Mesh == nullptr || Instance == nullptr || Shader == nullptr ||
                Context.Shaders->GetVertexShader(*Shader) == nullptr ||
                Context.Shaders->GetPixelShader(*Shader) == nullptr ||
                FindMeshBatch(*Context.Batches, Item.Mesh, Item.Section) == nullptr)
            {
                Log::Error("Render item is missing a mesh, material, or shader");
                return false;
            }

            const MaterialParameterBlock* Parameter = Instance->FindParameters("ForwardRenderer");
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
            const float DirectDirection[4] = {Direct.Direction[0], Direct.Direction[1], Direct.Direction[2], 0.0F};
            const float DirectColor[4] = {Direct.Color[0], Direct.Color[1], Direct.Color[2], Direct.Intensity};
            const float PointPosition[4] = {Point.Position[0], Point.Position[1], Point.Position[2], Point.Radius};
            const float PointColor[4] = {Point.Color[0], Point.Color[1], Point.Color[2], Point.Intensity};
            const float SkyColor[4] = {Sky.Color[0], Sky.Color[1], Sky.Color[2], Sky.Intensity};
            MaterialParameter::TextureBinding Textures;
            std::map<std::string, std::array<float, 4>> ScalarValues;
            if (Parameter != nullptr)
            {
                Textures = MaterialParameter::GetTextureBinding(*Parameter);
                for (const auto& [Name, Scalar] : Parameter->Scalars)
                {
                    ScalarValues.emplace(Name, Scalar.Value);
                }
            }
            const auto ResolveTexture = [&](TextureAssetHandle Handle, uint32& Index) -> bool
            {
                if (!Handle.IsValid())
                {
                    return true;
                }

                const auto Found = Context.TextureDescriptors->find(Handle.Index);
                if (Found == Context.TextureDescriptors->end())
                {
                    Log::Error("Material descriptor is missing");
                    return false;
                }

                Index = Found->second;
                return true;
            };
            uint32 SamplerDescriptor = InvalidShaderDescriptor;
            std::map<std::string, uint32> TextureIndices;
            for (const auto& [Name, Handle] : Textures.Textures)
            {
                uint32 Index = InvalidShaderDescriptor;
                if (!ResolveTexture(Handle, Index))
                {
                    return false;
                }

                TextureIndices.emplace(Name, Index);
            }
            if (Textures.Sampler.IsValid())
            {
                const auto Sampler = Context.Samplers->find(Textures.Sampler.Index);
                if (Sampler == Context.Samplers->end() || Sampler->second == nullptr)
                {
                    Log::Error("Material descriptor is missing");
                    return false;
                }

                SamplerDescriptor = Sampler->second->GetDescriptorIndex();
            }

            uint8* Destination = Constants.data() + Index * Context.ConstantAlignment;
            std::memcpy(Destination, &Slot, sizeof(Slot));
            const MeshBatch* Batch = FindMeshBatch(*Context.Batches, Item.Mesh, Item.Section);
            if (Batch != nullptr)
            {
                Batch->Layout.WriteScalars(Destination, ScalarValues);
                Batch->Layout.WriteUniform(Destination, "ForwardDirectLight.Direction", DirectDirection);
                Batch->Layout.WriteUniform(Destination, "ForwardDirectLight.Color", DirectColor);
                Batch->Layout.WriteUniform(Destination, "ForwardPointLight.Position", PointPosition);
                Batch->Layout.WriteUniform(Destination, "ForwardPointLight.Color", PointColor);
                Batch->Layout.WriteUniform(Destination, "ForwardSkyLight.Color", SkyColor);
                Batch->Layout.WriteDescriptors(Destination, TextureIndices, SamplerDescriptor);
            }
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
