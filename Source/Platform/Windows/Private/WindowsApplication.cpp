#include "Platform/Windows/Public/WindowsApplication.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/Config/Public/ConfigManager.h"
#include "Framework/UI/Public/UIManager.h"

#include <string>

#include <windows.h>

#include "ThirdPartyAdapter/UI/Public/ImGuiAdapter.h"

namespace ShadowEngine
{
    namespace
    {
        constexpr wchar_t WindowClassName[] = L"ShadowEngineWindowClass";
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

            if ((Message == WM_KEYDOWN || Message == WM_KEYUP) && WParam == 'U')
            {
                if (Application != nullptr)
                {
                    const bool bRepeat = (LParam & (static_cast<LPARAM>(1) << 30)) != 0;
                    Application->OnKey(static_cast<uint32>(WParam), Message == WM_KEYDOWN, bRepeat);
                }
                return 0;
            }

            if (UIManager::Get().IsVisible() &&
                ImGuiAdapter::HasContext() &&
                ImGuiAdapter::Win32WndProc(Window, Message, WParam, LParam) != 0)
            {
                return 1;
            }

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
            UnregisterClassW(WindowClassName, Instance);
            return -1;
        }

        WindowHandle = Window;

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

        if (ImGuiAdapter::HasContext())
        {
            if (ImGuiAdapter::Win32Init(Window) == false)
            {
                Log::Error("ImGui Win32 initialization failed");
                Finalize();
                return -1;
            }
            bImGuiPlatform = true;
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
        if (!UIManager::Get().IsVisible())
        {
            CaptureMouse();
        }

        Log::Info("Camera ready (WASD move, E up, Q down, Esc releases the mouse, click the view to look, U toggles the panel)");
        return 0;
    }

    void WindowsApplication::OnWindowResized(uint32 Width, uint32 Height)
    {
        EngineInstance.Resize(Width, Height);
        EngineInstance.Tick(0.0F);
    }

    void WindowsApplication::OnKey(uint32 Key, bool bDown, bool bRepeat)
    {
        if (bDown && !bRepeat && Key == 'U' && UIManager::Get().IsActive())
        {
            UIManager::Get().ToggleVisible();
            if (UIManager::Get().IsVisible())
            {
                ReleaseMouse();
                ClearKeys();
            }
            else
            {
                CaptureMouse();
            }
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
            case 'Q':
                bKeyDown = bPressed;
                break;
            case 'E':
                bKeyUp = bPressed;
                break;
            default:
                break;
        }
    }

    void WindowsApplication::OnMouseDown()
    {
        if (UIManager::Get().IsVisible() || bMouseCaptured)
        {
            return;
        }

        CaptureMouse();
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
        bKeyUp = false;
        bKeyDown = false;
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

    void WindowsApplication::Finalize()
    {
        ReleaseMouse();

        if (bImGuiPlatform)
        {
            ImGuiAdapter::Win32Shutdown();
            bImGuiPlatform = false;
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
        float Up = 0.0F;
        float Yaw = 0.0F;
        float Pitch = 0.0F;
        if (!UIManager::Get().IsVisible())
        {
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
            if (bKeyUp)
            {
                Up += 1.0F;
            }
            if (bKeyDown)
            {
                Up -= 1.0F;
            }
            if (bMouseCaptured)
            {
                SampleMouseLook(Yaw, Pitch);
            }
        }

        EngineInstance.SetCameraMotion(Forward, Right, Up, Yaw, Pitch);
        BaseApplication::Tick(FrameTime);
    }

    IApplication* G_App = new WindowsApplication();
}
