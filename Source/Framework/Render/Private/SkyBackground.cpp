#include "Framework/Render/Public/SkyBackground.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "Framework/RHI/Public/RHISwapChain.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <cstring>

namespace ShadowEngine
{
    namespace
    {
        struct GpuSkyConstants
        {
            float InverseViewProjection[16];
            float CameraPosition[4];
            float Viewport[4];
            uint32 Environment;
            uint32 Sampler;
            uint32 Pad0;
            uint32 Pad1;
        };
    }

    void ApplyProjectionJitter(glm::mat4& Projection, float JitterX, float JitterY, float Width, float Height)
    {
        if (Width <= 0.0F || Height <= 0.0F)
        {
            return;
        }

        Projection[2][0] += (2.0F * JitterX) / Width;
        Projection[2][1] += (2.0F * JitterY) / Height;
    }

    bool DrawSkyBackground(const RenderPassContext& Context)
    {
        static_assert(sizeof(GpuSkyConstants) <= 256);
        if (Context.CommandList == nullptr || Context.SkyPipeline == nullptr || Context.SkyConstants == nullptr ||
            Context.SwapChain == nullptr || Context.View == nullptr)
        {
            return false;
        }

        const float Width = static_cast<float>(Context.SwapChain->GetWidth());
        const float Height = static_cast<float>(Context.SwapChain->GetHeight());
        const float Aspect = Height > 0.0F ? Width / Height : 1.0F;
        const glm::mat4 View = Context.View->ViewMatrix();
        glm::mat4 Projection = glm::perspectiveRH_ZO(glm::radians(45.0F), Aspect, 0.1F, 100.0F);
        ApplyProjectionJitter(Projection, Context.ProjectionJitterX, Context.ProjectionJitterY, Width, Height);
        const glm::mat4 InverseViewProjection = glm::inverse(Projection * View);
        const glm::vec3 CameraPosition = Context.View->GetPosition();

        GpuSkyConstants Constants{};
        std::memcpy(Constants.InverseViewProjection, glm::value_ptr(InverseViewProjection), sizeof(Constants.InverseViewProjection));
        Constants.CameraPosition[0] = CameraPosition.x;
        Constants.CameraPosition[1] = CameraPosition.y;
        Constants.CameraPosition[2] = CameraPosition.z;
        Constants.Viewport[0] = Width;
        Constants.Viewport[1] = Height;
        Constants.Environment = Context.EnvironmentDescriptor;
        Constants.Sampler = Context.EnvironmentSampler;
        if (!Context.SkyConstants->Update(0, {reinterpret_cast<const uint8*>(&Constants), sizeof(Constants)}))
        {
            Log::Error("Failed to write sky constants");
            return false;
        }

        Context.CommandList->SetPipeline(*Context.SkyPipeline);
        Context.CommandList->BindShaderResources();
        Context.CommandList->SetConstantBuffer(*Context.SkyConstants, 0);
        Context.CommandList->Draw(3);
        return true;
    }
}
