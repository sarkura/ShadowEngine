#include "Framework/Engine/Public/Engine.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/Config/Public/ConfigManager.h"
#include "Framework/Engine/Public/EngineModules.h"

namespace ShadowEngine
{
    namespace
    {
        constexpr char ShaderDirectory[] = "Shaders";

        RHIColor ToClearColor(const std::vector<float>& Color)
        {
            RHIColor Result;
            if (Color.size() >= 3)
            {
                Result.R = Color[0];
                Result.G = Color[1];
                Result.B = Color[2];
            }
            if (Color.size() >= 4)
            {
                Result.A = Color[3];
            }
            return Result;
        }
    }

    Engine::~Engine()
    {
        Finalize();
    }

    bool Engine::Initialize(const EngineInitDesc& Desc, std::string* ErrorMessage)
    {
        if (bInitialized)
        {
            return true;
        }

        if (!InitializeRHI(Desc, ErrorMessage))
        {
            Finalize();
            return false;
        }

        Shaders = std::make_unique<ShaderManager>();
        if (!Shaders->Initialize(
                EngineModules::Get().CreateShaderCompiler(),
                ShaderDirectory,
                ErrorMessage))
        {
            Finalize();
            return false;
        }

        const EngineSetting::ViewportSetting Viewport =
            ConfigManager::Get().GetViewportSetting();

        MainRenderer = std::make_unique<Renderer>();
        if (!MainRenderer->Initialize(
                *Device,
                *SwapChain,
                *Shaders,
                ToClearColor(Viewport.BackgroundColor),
                ErrorMessage))
        {
            Finalize();
            return false;
        }

        bInitialized = true;
        Log::Info("Engine initialized");
        return true;
    }

    bool Engine::InitializeRHI(const EngineInitDesc& Desc, std::string* ErrorMessage)
    {
        const RenderSetting::RHISetting Setting = ConfigManager::Get().GetRHISetting();

        Device = EngineModules::Get().CreateRHIDevice(Setting.Backend);
        if (Device == nullptr)
        {
            SetErrorMessage(
                ErrorMessage,
                "RHI backend is not available: " + Setting.Backend);
            return false;
        }

        RHIDeviceDesc DeviceDesc;
        DeviceDesc.bEnableDebugLayer = Setting.bDebugLayer;
        if (!Device->Initialize(DeviceDesc, ErrorMessage))
        {
            return false;
        }

        const RHIAdapter* Adapter = Device->GetAdapter();
        Log::Info(
            "RHI {} initialized on {}",
            Device->GetName(),
            Adapter != nullptr ? Adapter->GetInfo().Name : "unknown adapter");

        RHISwapChainDesc SwapChainDesc;
        SwapChainDesc.WindowHandle = Desc.WindowHandle;
        SwapChainDesc.Width = Desc.Width;
        SwapChainDesc.Height = Desc.Height;
        SwapChainDesc.BufferCount = static_cast<uint32>(Setting.BackBufferCount);
        SwapChainDesc.Format = ERHIFormat::R8G8B8A8_UNorm;
        SwapChainDesc.bVSync = Setting.bVSync;
        SwapChain = Device->CreateSwapChain(SwapChainDesc, ErrorMessage);
        if (SwapChain == nullptr)
        {
            return false;
        }

        Log::Info("Swap chain created ({}x{})", Desc.Width, Desc.Height);
        return true;
    }

    void Engine::Finalize()
    {
        if (Device != nullptr)
        {
            Device->WaitIdle();
        }

        MainRenderer.reset();
        if (Shaders != nullptr)
        {
            Shaders->Finalize();
            Shaders.reset();
        }
        SwapChain.reset();
        if (Device != nullptr)
        {
            Device->Finalize();
            Device.reset();
        }

        bInitialized = false;
        bRenderingPaused = false;
    }

    void Engine::Tick(float DeltaTime)
    {
        (void)DeltaTime;

        if (!bInitialized || bRenderingPaused)
        {
            return;
        }

        if (!MainRenderer->RenderFrame())
        {
            Log::Error("Frame presentation failed, stopping rendering");
            bInitialized = false;
        }
    }

    void Engine::Resize(uint32 Width, uint32 Height)
    {
        if (!bInitialized)
        {
            return;
        }

        if (Width == 0 || Height == 0)
        {
            bRenderingPaused = true;
            return;
        }

        bRenderingPaused = false;
        if (Width == SwapChain->GetWidth() && Height == SwapChain->GetHeight())
        {
            return;
        }

        Device->WaitIdle();

        std::string ErrorMessage;
        if (!SwapChain->Resize(Width, Height, &ErrorMessage))
        {
            Log::Error("Swap chain resize failed, stopping rendering: {}", ErrorMessage);
            bInitialized = false;
        }
    }

    bool Engine::IsInitialized() const
    {
        return bInitialized;
    }

    bool Engine::IsRenderingPaused() const
    {
        return bRenderingPaused;
    }
}
