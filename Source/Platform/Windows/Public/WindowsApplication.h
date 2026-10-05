#pragma once

#include "Framework/Common/Public/BaseApplication.h"
#include "Framework/Common/Public/Types.h"

namespace ShadowEngine
{
    class WindowsApplication final : public BaseApplication
    {
        public:
            WindowsApplication();
            ~WindowsApplication() override;

            int Initialize() override;
            void Finalize() override;
            void Tick(float DeltaTime) override;

            void OnWindowResized(uint32 Width, uint32 Height);
            void OnWindowMoved();
            void OnKey(uint32 Key, bool bDown, bool bRepeat);
            void OnMouseDown();
            void OnFocusLost();

        private:
            void CaptureMouse();
            void ReleaseMouse();
            void ClearKeys();
            void SampleMouseLook(float& Yaw, float& Pitch);
            float AdvanceTime();
            void UpdateFpsOverlay(float DeltaTime);
            void PlaceFpsOverlay();
            void DrawFpsOverlay(int Fps);
            void ToggleFps();

            void* WindowHandle = nullptr;
            void* ModuleHandle = nullptr;
            void* FpsWindow = nullptr;
            void* FpsFont = nullptr;
            bool bShowFps = true;
            bool bMouseCaptured = false;
            bool bCursorHidden = false;
            bool bKeyForward = false;
            bool bKeyBack = false;
            bool bKeyLeft = false;
            bool bKeyRight = false;
            bool bKeyUp = false;
            bool bKeyDown = false;
            int64 TimeFrequency = 0;
            int64 TimeLast = 0;
            float FpsAccumulated = 0.0F;
            int FpsFrames = 0;
            int DisplayedFps = 0;
    };
}
