#include "Framework/Engine/Public/Engine.h"

#include "Framework/Asset/Public/MeshAsset.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/Config/Public/ConfigManager.h"
#include "Framework/Engine/Public/EngineModules.h"
#include "Framework/Render/Public/BlinnPhongRenderer.h"
#include "Framework/Render/Public/DebugRenderer.h"
#include "Framework/Scene/Public/Light.h"
#include "Framework/Scene/Public/Scene.h"

#include <utility>
#include <vector>

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

        void AddMesh(
            Scene& InScene,
            MeshHandle Handle,
            const MeshAsset& Mesh,
            const SceneSetting::Transform& InTransform)
        {
            std::vector<MaterialInstanceHandle> Materials;
            Materials.reserve(Mesh.GetSections().size());
            for (const MeshSection& Section : Mesh.GetSections())
            {
                Materials.push_back(Section.Material);
            }

            Entity& Object = InScene.CreateEntity();
            Object.SetMesh(Handle, std::move(Materials));
            Object.GetTransform().SetTranslation(
                InTransform.Translation[0],
                InTransform.Translation[1],
                InTransform.Translation[2]);
            Object.GetTransform().SetRotation(
                InTransform.Rotation[0],
                InTransform.Rotation[1],
                InTransform.Rotation[2]);
            Object.GetTransform().SetScale(
                InTransform.Scale[0],
                InTransform.Scale[1],
                InTransform.Scale[2]);
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

        Assets = std::make_unique<AssetManager>();
        Assets->SetShaderManager(*Shaders);

        const SceneSetting::Setting SceneConfig = ConfigManager::Get().GetSceneSetting();
        MainScene = std::make_unique<Scene>();
        uint32 InstanceCount = 0;
        for (const SceneSetting::Mesh& MeshObject : SceneConfig.Meshes)
        {
            const MeshHandle Mesh = Assets->LoadMesh(MeshObject.Path, ErrorMessage);
            const MeshAsset* Asset = Assets->ResolveMesh(Mesh);
            if (!Mesh.IsValid() || Asset == nullptr)
            {
                Finalize();
                return false;
            }

            for (const SceneSetting::MeshInstance& Instance : MeshObject.Instances)
            {
                AddMesh(*MainScene, Mesh, *Asset, Instance.Transform);
                ++InstanceCount;
            }
        }
        Log::Info(
            "Scene created ({} meshes, {} instances, {} lights)",
            SceneConfig.Meshes.size(),
            InstanceCount,
            SceneConfig.Lights.size());

        for (const SceneSetting::Light& LightConfig : SceneConfig.Lights)
        {
            DirectLight& Light = MainScene->CreateDirectLight();
            Light.SetDirection(LightConfig.Direction[0], LightConfig.Direction[1], LightConfig.Direction[2]);
            Light.SetColor(LightConfig.Color[0], LightConfig.Color[1], LightConfig.Color[2]);
            Light.SetIntensity(LightConfig.Intensity);
        }

        const std::string RendererName = ConfigManager::Get().GetRHISetting().Renderer;
        if (RendererName == "BlinnPhongRenderer")
        {
            MainRenderer = std::make_unique<BlinnPhongRenderer>();
        }
        else if (RendererName == "DebugRenderer")
        {
            MainRenderer = std::make_unique<DebugRenderer>();
        }
        else
        {
            SetErrorMessage(ErrorMessage, "Renderer is not available: " + RendererName);
            Finalize();
            return false;
        }
        if (!MainRenderer->Initialize(
                *Device,
                *SwapChain,
                *Shaders,
                *Assets,
                *MainScene,
                ToClearColor(Viewport.BackgroundColor),
                ErrorMessage))
        {
            Finalize();
            return false;
        }

        const EngineSetting::MovementSetting Movement =
            ConfigManager::Get().GetMovementSetting();
        MainRenderer->SetCameraMoveSpeed(
            Movement.ForwardSpeed,
            Movement.RightSpeed,
            Movement.UpSpeed,
            Movement.DownSpeed);

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
        MainScene.reset();
        if (Assets != nullptr)
        {
            Assets->Finalize();
            Assets.reset();
        }
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

    void Engine::SetCameraMotion(float Forward, float Right, float Up, float Yaw, float Pitch)
    {
        CameraForward = Forward;
        CameraRight = Right;
        CameraUp = Up;
        CameraYaw = Yaw;
        CameraPitch = Pitch;
    }

    void Engine::Tick(float DeltaTime)
    {
        if (!bInitialized || bRenderingPaused || MainRenderer == nullptr)
        {
            CameraForward = 0.0F;
            CameraRight = 0.0F;
            CameraUp = 0.0F;
            CameraYaw = 0.0F;
            CameraPitch = 0.0F;
            return;
        }

        MainRenderer->UpdateCamera(DeltaTime, CameraForward, CameraRight, CameraUp, CameraYaw, CameraPitch);
        CameraForward = 0.0F;
        CameraRight = 0.0F;
        CameraUp = 0.0F;
        CameraYaw = 0.0F;
        CameraPitch = 0.0F;

        MainRenderer->Sync(*MainScene);
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
        if (!SwapChain->Resize(Width, Height, &ErrorMessage) ||
            !MainRenderer->Resize(Width, Height, &ErrorMessage))
        {
            Log::Error("Resize failed, stopping rendering: {}", ErrorMessage);
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
