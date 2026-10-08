#include "Framework/Render/Public/BlinnPhongRenderer.h"

#include "Framework/Asset/Public/AssetManager.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/Material/Public/MaterialInstance.h"
#include "Framework/Material/Public/MaterialParameter.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/RHI/Public/RHISwapChain.h"

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
        };

    }

    bool BlinnPhongRenderer::RenderFrame()
    {
        static_assert(sizeof(GpuObjectConstants) <= ConstantAlignment);
        if (SwapChain == nullptr)
        {
            return false;
        }

        const float Width = static_cast<float>(SwapChain->GetWidth());
        const float Height = static_cast<float>(SwapChain->GetHeight());
        const float Aspect = Height > 0.0F ? Width / Height : 1.0F;
        const glm::mat4 View = ViewCamera.ViewMatrix();
        const glm::mat4 Projection = glm::perspectiveRH_ZO(glm::radians(45.0F), Aspect, 0.1F, 100.0F);
        const glm::mat4 ViewProjection = Projection * View;
        const glm::vec3 CameraPosition = ViewCamera.GetPosition();

        std::vector<FrameDraw> Draws;
        if (!CollectDraws(Draws))
        {
            return false;
        }

        if (Draws.size() > ConstantCapacity)
        {
            Log::Error("Scene has more objects than the constant buffer can hold");
            return false;
        }

        DirectLightRenderProxy Light{};
        if (!SceneProxy.DirectLights.empty())
        {
            Light = SceneProxy.DirectLights.front();
        }

        std::vector<uint8> Constants(Draws.size() * ConstantAlignment);
        for (size_t Index = 0; Index < Draws.size(); ++Index)
        {
            const MaterialInstance* Instance = Assets->ResolveMaterialInstance(Draws[Index].Material);
            if (Instance == nullptr)
            {
                Log::Error("Render proxy material is missing");
                return false;
            }

            const MaterialParameterBlock& Parameter = Instance->GetParameters();
            GpuObjectConstants Slot{};
            std::memcpy(Slot.ViewProjection, glm::value_ptr(ViewProjection), sizeof(Slot.ViewProjection));
            std::memcpy(Slot.World, Draws[Index].Proxy->World, sizeof(Slot.World));
            std::memcpy(Slot.NormalMatrix, Draws[Index].Proxy->NormalMatrix, sizeof(Slot.NormalMatrix));
            Slot.CameraPosition[0] = CameraPosition.x;
            Slot.CameraPosition[1] = CameraPosition.y;
            Slot.CameraPosition[2] = CameraPosition.z;
            Slot.LightDirection[0] = Light.Direction[0];
            Slot.LightDirection[1] = Light.Direction[1];
            Slot.LightDirection[2] = Light.Direction[2];
            Slot.LightColor[0] = Light.Color[0];
            Slot.LightColor[1] = Light.Color[1];
            Slot.LightColor[2] = Light.Color[2];
            Slot.LightColor[3] = Light.Intensity;
            MaterialParameter::WriteSurfaceConstants(Parameter, Slot.Surface);
            const MaterialParameter::TextureBinding Textures = MaterialParameter::GetTextureBinding(Parameter);
            if (Textures.HasTextures())
            {
                const auto BaseColor = TextureDescriptors.find(Textures.BaseColor.Index);
                const auto Roughness = TextureDescriptors.find(Textures.Roughness.Index);
                const auto Normal = TextureDescriptors.find(Textures.Normal.Index);
                const auto Sampler = GpuSamplers.find(Textures.Sampler.Index);
                if (BaseColor == TextureDescriptors.end() || Roughness == TextureDescriptors.end() ||
                    Normal == TextureDescriptors.end() || Sampler == GpuSamplers.end() || Sampler->second == nullptr)
                {
                    Log::Error("Material descriptor is missing");
                    return false;
                }

                Slot.BaseColorDescriptor = BaseColor->second;
                Slot.RoughnessDescriptor = Roughness->second;
                Slot.NormalDescriptor = Normal->second;
                Slot.SamplerDescriptor = Sampler->second->GetDescriptorIndex();
            }
            std::memcpy(Constants.data() + Index * ConstantAlignment, &Slot, sizeof(Slot));
        }
        if (!Constants.empty() && !ConstantBuffer->Update(0, Constants))
        {
            Log::Error("Failed to write scene constants");
            return false;
        }

        return SubmitDraws(Draws);
    }

    const char* BlinnPhongRenderer::GetName() const
    {
        return "BlinnPhongRenderer";
    }
}
