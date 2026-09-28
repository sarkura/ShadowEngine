#include "Platform/Windows/Public/WindowsApplication.h"

#include "Framework/Config/Public/ConfigManager.h"

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
            if (Message == WM_DESTROY)
            {
                PostQuitMessage(0);
                return 0;
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

        const ViewportSetting Viewport =
            ConfigManager::Get().GetViewportSetting();

        WNDCLASSEXW WindowClass{};
        WindowClass.cbSize = sizeof(WindowClass);
        WindowClass.style = CS_HREDRAW | CS_VREDRAW;
        WindowClass.lpfnWndProc = WindowProcedure;
        WindowClass.hInstance = Instance;
        WindowClass.hCursor =
            LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        WindowClass.hbrBackground =
            reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
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
            nullptr);

        if (Window == nullptr)
        {
            UnregisterClassW(WindowClassName, Instance);
            return -1;
        }

        WindowHandle = Window;
        ShowWindow(Window, SW_SHOW);
        UpdateWindow(Window);
        return 0;
    }

    void WindowsApplication::Finalize()
    {
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
        BaseApplication::Finalize();
    }

    void WindowsApplication::Tick(float DeltaTime)
    {
        BaseApplication::Tick(DeltaTime);

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
    }

    IApplication* G_App = new WindowsApplication();
}