#include "Framework/Render/Public/DebugRenderer.h"

#include "Framework/Common/Public/Log.h"
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
        };
    }

    bool DebugRenderer::RenderFrame()
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

        std::vector<uint8> Constants(Draws.size() * ConstantAlignment);
        for (size_t Index = 0; Index < Draws.size(); ++Index)
        {
            GpuObjectConstants Slot{};
            std::memcpy(Slot.ViewProjection, glm::value_ptr(ViewProjection), sizeof(Slot.ViewProjection));
            std::memcpy(Slot.World, Draws[Index].Proxy->World, sizeof(Slot.World));
            std::memcpy(Slot.NormalMatrix, Draws[Index].Proxy->NormalMatrix, sizeof(Slot.NormalMatrix));
            std::memcpy(Constants.data() + Index * ConstantAlignment, &Slot, sizeof(Slot));
        }
        if (!Constants.empty() && !ConstantBuffer->Update(0, Constants))
        {
            Log::Error("Failed to write scene constants");
            return false;
        }

        return SubmitDraws(Draws);
    }

    const char* DebugRenderer::GetName() const
    {
        return "DebugRenderer";
    }
}
