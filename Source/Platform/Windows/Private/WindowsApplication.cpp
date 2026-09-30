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

        if (IsIconic(Window))
        {
            EngineInstance.Resize(0, 0);
        }

        return 0;
    }

    void WindowsApplication::OnWindowResized(uint32 Width, uint32 Height)
    {
        EngineInstance.Resize(Width, Height);
        EngineInstance.Tick(0.0F);
    }

    void WindowsApplication::Finalize()
    {
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

        if (!bQuit)
        {
            BaseApplication::Tick(DeltaTime);
        }
    }

    IApplication* G_App = new WindowsApplication();
}