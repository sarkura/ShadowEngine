#include "Platform/Windows/Public/WindowsApplication.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/Config/Public/ConfigManager.h"

#include <string>

#include <windows.h>

namespace ShadowEngine
{
    namespace
    {
        constexpr wchar_t WindowClassName[] = L"ShadowEngineWindowClass";
        constexpr wchar_t FpsClassName[] = L"ShadowEngineFpsWindow";
        constexpr int FpsWidth = 88;
        constexpr int FpsHeight = 32;
        constexpr int FpsMargin = 12;
        constexpr float LookRadiansPerSpeed = 0.00025F;
        constexpr float MaxFrameTime = 0.1F;

        LRESULT CALLBACK WindowProcedure(
            HWND Window,
            UINT Message,
            WPARAM WParam,
            LPARAM LParam)
        {
            if (Message == WM_NCCREATE)
            {
                const auto* CreateInfo = reinterpret_cast<const CREATESTRUCTW*>(LParam);
                SetWindowLongPtrW(
                    Window,
                    GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(CreateInfo->lpCreateParams));
            }

            auto* Application = reinterpret_cast<WindowsApplication*>(
                GetWindowLongPtrW(Window, GWLP_USERDATA));

            switch (Message)
            {
                case WM_SIZE:
                    if (Application != nullptr)
                    {
                        Application->OnWindowResized(
                            static_cast<uint32>(LOWORD(LParam)),
                            static_cast<uint32>(HIWORD(LParam)));
                    }
                    return 0;

                case WM_MOVE:
                    if (Application != nullptr)
                    {
                        Application->OnWindowMoved();
                    }
                    break;

                case WM_LBUTTONDOWN:
                    if (Application != nullptr)
                    {
                        Application->OnMouseDown();
                    }
                    return 0;

                case WM_KEYDOWN:
                    if (Application != nullptr)
                    {
                        const bool bRepeat = (LParam & (static_cast<LPARAM>(1) << 30)) != 0;
                        Application->OnKey(static_cast<uint32>(WParam), true, bRepeat);
                    }
                    return 0;

                case WM_KEYUP:
                    if (Application != nullptr)
                    {
                        Application->OnKey(static_cast<uint32>(WParam), false, false);
                    }
                    return 0;

                case WM_KILLFOCUS:
                case WM_ACTIVATE:
                    if (Application != nullptr &&
                        (Message == WM_KILLFOCUS || LOWORD(WParam) == WA_INACTIVE))
                    {
                        Application->OnFocusLost();
                    }
                    break;

                case WM_DESTROY:
                    SetWindowLongPtrW(Window, GWLP_USERDATA, 0);
                    PostQuitMessage(0);
                    return 0;

                default:
                    break;
            }

            return DefWindowProcW(Window, Message, WParam, LParam);
        }
    }

    WindowsApplication::WindowsApplication() = default;

    WindowsApplication::~WindowsApplication() = default;

    int WindowsApplication::Initialize()
    {
        if (BaseApplication::Initialize() != 0)
        {
            return -1;
        }

        const HINSTANCE Instance = GetModuleHandleW(nullptr);
        ModuleHandle = Instance;

        const EngineSetting::ViewportSetting Viewport =
            ConfigManager::Get().GetViewportSetting();

        WNDCLASSEXW WindowClass{};
        WindowClass.cbSize = sizeof(WindowClass);
        WindowClass.style = CS_HREDRAW | CS_VREDRAW;
        WindowClass.lpfnWndProc = WindowProcedure;
        WindowClass.hInstance = Instance;
        WindowClass.hCursor =
            LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        WindowClass.hbrBackground = nullptr;
        WindowClass.lpszClassName = WindowClassName;

        if (RegisterClassExW(&WindowClass) == 0)
        {
            return -1;
        }

        WNDCLASSEXW FpsClass{};
        FpsClass.cbSize = sizeof(FpsClass);
        FpsClass.lpfnWndProc = DefWindowProcW;
        FpsClass.hInstance = Instance;
        FpsClass.lpszClassName = FpsClassName;
        if (RegisterClassExW(&FpsClass) == 0)
        {
            UnregisterClassW(WindowClassName, Instance);
            return -1;
        }

        RECT WindowRectangle{
            0,
            0,
            Viewport.Width,
            Viewport.Height};
        if (!AdjustWindowRectEx(
                &WindowRectangle,
                WS_OVERLAPPEDWINDOW,
                FALSE,
                0))
        {
            UnregisterClassW(FpsClassName, Instance);
            UnregisterClassW(WindowClassName, Instance);
            return -1;
        }

        const HWND Window = CreateWindowExW(
            0,
            WindowClassName,
            L"ShadowEngine",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            WindowRectangle.right - WindowRectangle.left,
            WindowRectangle.bottom - WindowRectangle.top,
            nullptr,
            nullptr,
            Instance,
            this);

        if (Window == nullptr)
        {
            UnregisterClassW(FpsClassName, Instance);
            UnregisterClassW(WindowClassName, Instance);
            return -1;
        }

        WindowHandle = Window;

        const HWND Overlay = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            FpsClassName,
            L"",
            WS_POPUP,
            0,
            0,
            FpsWidth,
            FpsHeight,
            Window,
            nullptr,
            Instance,
            nullptr);
        FpsWindow = Overlay;
        if (Overlay != nullptr)
        {
            FpsFont = CreateFontW(
                -22,
                0,
                0,
                0,
                FW_NORMAL,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI");
        }
        else
        {
            Log::Error("FPS overlay window was not created");
        }

        ShowWindow(Window, SW_SHOW);
        UpdateWindow(Window);

        RECT ClientRectangle{};
        GetClientRect(Window, &ClientRectangle);

        EngineInitDesc EngineDesc;
        EngineDesc.WindowHandle = Window;
        EngineDesc.Width = static_cast<uint32>(ClientRectangle.right - ClientRectangle.left);
        EngineDesc.Height = static_cast<uint32>(ClientRectangle.bottom - ClientRectangle.top);
        if (EngineDesc.Width == 0 || EngineDesc.Height == 0)
        {
            EngineDesc.Width = static_cast<uint32>(Viewport.Width);
            EngineDesc.Height = static_cast<uint32>(Viewport.Height);
        }

        std::string ErrorMessage;
        if (!EngineInstance.Initialize(EngineDesc, &ErrorMessage))
        {
            Log::Error("Engine initialization failed: {}", ErrorMessage);
            Finalize();
            return -1;
        }

        if (IsIconic(Window))
        {
            EngineInstance.Resize(0, 0);
        }

        LARGE_INTEGER Frequency{};
        LARGE_INTEGER Now{};
        QueryPerformanceFrequency(&Frequency);
        QueryPerformanceCounter(&Now);
        TimeFrequency = Frequency.QuadPart;
        TimeLast = Now.QuadPart;

        SetForegroundWindow(Window);
        CaptureMouse();
        if (Overlay != nullptr)
        {
            DrawFpsOverlay(0);
            ShowWindow(Overlay, SW_SHOWNOACTIVATE);
        }

        Log::Info("Camera ready (WASD move, mouse look, U toggles FPS, Esc releases mouse)");
        return 0;
    }

    void WindowsApplication::OnWindowResized(uint32 Width, uint32 Height)
    {
        EngineInstance.Resize(Width, Height);
        PlaceFpsOverlay();
        EngineInstance.Tick(0.0F);
    }

    void WindowsApplication::OnWindowMoved()
    {
        PlaceFpsOverlay();
    }

    void WindowsApplication::OnKey(uint32 Key, bool bDown, bool bRepeat)
    {
        if (bDown && !bRepeat && Key == 'U')
        {
            ToggleFps();
        }
        if (bDown && !bRepeat && Key == VK_ESCAPE)
        {
            ReleaseMouse();
        }

        const bool bPressed = bDown;
        switch (Key)
        {
            case 'W':
                bKeyForward = bPressed;
                break;
            case 'S':
                bKeyBack = bPressed;
                break;
            case 'A':
                bKeyLeft = bPressed;
                break;
            case 'D':
                bKeyRight = bPressed;
                break;
            default:
                break;
        }
    }

    void WindowsApplication::OnMouseDown()
    {
        if (!bMouseCaptured)
        {
            CaptureMouse();
        }
    }

    void WindowsApplication::OnFocusLost()
    {
        ReleaseMouse();
        ClearKeys();
    }

    void WindowsApplication::CaptureMouse()
    {
        const HWND Window = static_cast<HWND>(WindowHandle);
        if (Window == nullptr || IsIconic(Window))
        {
            return;
        }

        RECT Client{};
        GetClientRect(Window, &Client);
        POINT TopLeft{Client.left, Client.top};
        POINT BottomRight{Client.right, Client.bottom};
        ClientToScreen(Window, &TopLeft);
        ClientToScreen(Window, &BottomRight);
        const RECT Clip{TopLeft.x, TopLeft.y, BottomRight.x, BottomRight.y};
        ClipCursor(&Clip);

        POINT Center{
            (Client.left + Client.right) / 2,
            (Client.top + Client.bottom) / 2};
        ClientToScreen(Window, &Center);
        SetCursorPos(Center.x, Center.y);

        if (!bCursorHidden)
        {
            ShowCursor(FALSE);
            bCursorHidden = true;
        }
        bMouseCaptured = true;
    }

    void WindowsApplication::ReleaseMouse()
    {
        if (!bMouseCaptured && !bCursorHidden)
        {
            return;
        }

        ClipCursor(nullptr);
        if (bCursorHidden)
        {
            ShowCursor(TRUE);
            bCursorHidden = false;
        }
        bMouseCaptured = false;
    }

    void WindowsApplication::ClearKeys()
    {
        bKeyForward = false;
        bKeyBack = false;
        bKeyLeft = false;
        bKeyRight = false;
    }

    void WindowsApplication::SampleMouseLook(float& Yaw, float& Pitch)
    {
        Yaw = 0.0F;
        Pitch = 0.0F;

        const HWND Window = static_cast<HWND>(WindowHandle);
        if (Window == nullptr)
        {
            return;
        }

        RECT Client{};
        GetClientRect(Window, &Client);
        POINT Center{
            (Client.left + Client.right) / 2,
            (Client.top + Client.bottom) / 2};
        POINT Cursor{};
        GetCursorPos(&Cursor);
        ScreenToClient(Window, &Cursor);

        const int DeltaX = Cursor.x - Center.x;
        const int DeltaY = Cursor.y - Center.y;
        ClientToScreen(Window, &Center);
        SetCursorPos(Center.x, Center.y);

        const EngineSetting::MovementSetting Movement = ConfigManager::Get().GetMovementSetting();
        Yaw = -static_cast<float>(DeltaX) * Movement.YawSpeed * LookRadiansPerSpeed;
        Pitch = -static_cast<float>(DeltaY) * Movement.PitchSpeed * LookRadiansPerSpeed;
    }

    float WindowsApplication::AdvanceTime()
    {
        if (TimeFrequency == 0)
        {
            return 0.0F;
        }

        LARGE_INTEGER Now{};
        QueryPerformanceCounter(&Now);
        const int64 Elapsed = Now.QuadPart - TimeLast;
        TimeLast = Now.QuadPart;
        float DeltaTime = static_cast<float>(Elapsed) / static_cast<float>(TimeFrequency);
        if (DeltaTime < 0.0F)
        {
            DeltaTime = 0.0F;
        }
        if (DeltaTime > MaxFrameTime)
        {
            DeltaTime = MaxFrameTime;
        }
        return DeltaTime;
    }

    void WindowsApplication::UpdateFpsOverlay(float DeltaTime)
    {
        if (FpsWindow == nullptr)
        {
            return;
        }

        FpsAccumulated += DeltaTime;
        ++FpsFrames;
        if (FpsAccumulated < 0.25F)
        {
            return;
        }

        const int Fps = static_cast<int>(static_cast<float>(FpsFrames) / FpsAccumulated + 0.5F);
        FpsAccumulated = 0.0F;
        FpsFrames = 0;
        if (Fps == DisplayedFps)
        {
            return;
        }

        DisplayedFps = Fps;
        if (bShowFps)
        {
            DrawFpsOverlay(Fps);
        }
    }

    void WindowsApplication::PlaceFpsOverlay()
    {
        const HWND Overlay = static_cast<HWND>(FpsWindow);
        const HWND Window = static_cast<HWND>(WindowHandle);
        if (Overlay == nullptr || Window == nullptr || !bShowFps)
        {
            return;
        }

        POINT Origin{FpsMargin, FpsMargin};
        ClientToScreen(Window, &Origin);
        UpdateLayeredWindow(Overlay, nullptr, &Origin, nullptr, nullptr, nullptr, 0, nullptr, 0);
    }

    void WindowsApplication::DrawFpsOverlay(int Fps)
    {
        const HWND Overlay = static_cast<HWND>(FpsWindow);
        const HWND Window = static_cast<HWND>(WindowHandle);
        if (Overlay == nullptr || Window == nullptr)
        {
            return;
        }

        HDC Screen = GetDC(nullptr);
        HDC Memory = CreateCompatibleDC(Screen);
        BITMAPINFO Info{};
        Info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        Info.bmiHeader.biWidth = FpsWidth;
        Info.bmiHeader.biHeight = -FpsHeight;
        Info.bmiHeader.biPlanes = 1;
        Info.bmiHeader.biBitCount = 32;
        Info.bmiHeader.biCompression = BI_RGB;

        void* Bits = nullptr;
        HBITMAP Bitmap = CreateDIBSection(Screen, &Info, DIB_RGB_COLORS, &Bits, nullptr, 0);
        if (Bitmap == nullptr || Bits == nullptr)
        {
            if (Bitmap != nullptr)
            {
                DeleteObject(Bitmap);
            }
            DeleteDC(Memory);
            ReleaseDC(nullptr, Screen);
            return;
        }

        const HGDIOBJ PreviousBitmap = SelectObject(Memory, Bitmap);
        RECT TextRect{8, 0, FpsWidth - 4, FpsHeight};
        HBRUSH Brush = CreateSolidBrush(RGB(0, 0, 0));
        RECT FillRectArea{0, 0, FpsWidth, FpsHeight};
        ::FillRect(Memory, &FillRectArea, Brush);
        DeleteObject(Brush);

        SetBkMode(Memory, TRANSPARENT);
        SetTextColor(Memory, RGB(255, 255, 255));
        if (FpsFont != nullptr)
        {
            SelectObject(Memory, static_cast<HFONT>(FpsFont));
        }

        wchar_t Text[16] = {};
        wsprintfW(Text, L"%d", Fps);
        DrawTextW(Memory, Text, -1, &TextRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        auto* Pixels = static_cast<uint32*>(Bits);
        const int PixelCount = FpsWidth * FpsHeight;
        for (int Index = 0; Index < PixelCount; ++Index)
        {
            const uint32 Pixel = Pixels[Index];
            const uint8 Blue = static_cast<uint8>(Pixel & 0xFF);
            const uint8 Green = static_cast<uint8>((Pixel >> 8) & 0xFF);
            const uint8 Red = static_cast<uint8>((Pixel >> 16) & 0xFF);
            const uint8 Alpha = (Red > 128 || Green > 128 || Blue > 128) ? 255 : 160;
            const uint8 OutRed = static_cast<uint8>(Red * Alpha / 255);
            const uint8 OutGreen = static_cast<uint8>(Green * Alpha / 255);
            const uint8 OutBlue = static_cast<uint8>(Blue * Alpha / 255);
            Pixels[Index] = (static_cast<uint32>(Alpha) << 24) |
                            (static_cast<uint32>(OutRed) << 16) |
                            (static_cast<uint32>(OutGreen) << 8) |
                            OutBlue;
        }

        POINT Origin{FpsMargin, FpsMargin};
        ClientToScreen(Window, &Origin);
        SIZE Size{FpsWidth, FpsHeight};
        POINT Source{0, 0};
        BLENDFUNCTION Blend{};
        Blend.BlendOp = AC_SRC_OVER;
        Blend.SourceConstantAlpha = 255;
        Blend.AlphaFormat = AC_SRC_ALPHA;
        UpdateLayeredWindow(Overlay, Screen, &Origin, &Size, Memory, &Source, 0, &Blend, ULW_ALPHA);

        SelectObject(Memory, PreviousBitmap);
        DeleteObject(Bitmap);
        DeleteDC(Memory);
        ReleaseDC(nullptr, Screen);
    }

    void WindowsApplication::ToggleFps()
    {
        bShowFps = !bShowFps;
        const HWND Overlay = static_cast<HWND>(FpsWindow);
        if (Overlay == nullptr)
        {
            return;
        }

        if (bShowFps)
        {
            DrawFpsOverlay(DisplayedFps);
            ShowWindow(Overlay, SW_SHOWNOACTIVATE);
        }
        else
        {
            ShowWindow(Overlay, SW_HIDE);
        }
    }

    void WindowsApplication::Finalize()
    {
        ReleaseMouse();

        const HWND Overlay = static_cast<HWND>(FpsWindow);
        if (Overlay != nullptr && IsWindow(Overlay))
        {
            DestroyWindow(Overlay);
        }
        FpsWindow = nullptr;

        if (FpsFont != nullptr)
        {
            DeleteObject(static_cast<HFONT>(FpsFont));
            FpsFont = nullptr;
        }

        BaseApplication::Finalize();

        const HWND Window = static_cast<HWND>(WindowHandle);
        if (Window != nullptr && IsWindow(Window))
        {
            DestroyWindow(Window);
        }

        const HINSTANCE Instance = static_cast<HINSTANCE>(ModuleHandle);
        if (Instance != nullptr)
        {
            UnregisterClassW(FpsClassName, Instance);
            UnregisterClassW(WindowClassName, Instance);
        }

        WindowHandle = nullptr;
        ModuleHandle = nullptr;
    }

    void WindowsApplication::Tick(float DeltaTime)
    {
        (void)DeltaTime;

        if (EngineInstance.IsRenderingPaused())
        {
            WaitMessage();
        }

        MSG Message{};
        while (PeekMessageW(&Message, nullptr, 0, 0, PM_REMOVE))
        {
            if (Message.message == WM_QUIT)
            {
                bQuit = true;
                break;
            }

            TranslateMessage(&Message);
            DispatchMessageW(&Message);
        }

        if (bQuit)
        {
            return;
        }

        const float FrameTime = AdvanceTime();
        float Forward = 0.0F;
        float Right = 0.0F;
        float Yaw = 0.0F;
        float Pitch = 0.0F;
        if (bKeyForward)
        {
            Forward += 1.0F;
        }
        if (bKeyBack)
        {
            Forward -= 1.0F;
        }
        if (bKeyRight)
        {
            Right += 1.0F;
        }
        if (bKeyLeft)
        {
            Right -= 1.0F;
        }
        if (bMouseCaptured)
        {
            SampleMouseLook(Yaw, Pitch);
        }

        EngineInstance.SetCameraMotion(Forward, Right, Yaw, Pitch);
        UpdateFpsOverlay(FrameTime);
        BaseApplication::Tick(FrameTime);
    }

    IApplication* G_App = new WindowsApplication();
}
