#include <iostream>
#include "Interface/Public/IApplication.hpp"
#include "ShaderCompiler/Public/ShaderCompiler.h"

#if SHADOW_WITH_D3D12
#include "RHI/Direct3D12/Public/D3D12RHIModule.h"
#endif

namespace ShadowEngine
{
    extern IApplication* G_App;

    namespace
    {
        void RegisterEngineModules()
        {
#if SHADOW_WITH_D3D12
            RegisterD3D12RHI();
#endif
            RegisterSlangShaderCompiler();
        }
    }
}


int main(int argc, char** argv) 
{
    ShadowEngine::RegisterEngineModules();

    int Ret = ShadowEngine::G_App->Initialize();
    if (Ret != 0)
    {
        std::cout << "App Initialize failed, exit!" << std::endl;
        return Ret;
    }

    while (!ShadowEngine::G_App->IsQuit())
    {
        ShadowEngine::G_App->Tick(0.0F);
    }

    ShadowEngine::G_App->Finalize();
    delete ShadowEngine::G_App;
    ShadowEngine::G_App = nullptr;

    return 0;
}
