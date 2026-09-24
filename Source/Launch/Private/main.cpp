#include <iostream>
#include "Interface/Public/IApplication.hpp"

namespace ShadowEngineInterface
{
    extern IApplication* G_App;
}


int main(int argc, char** argv) 
{
    int Ret = ShadowEngineInterface::G_App->Initialize();
    if (Ret != 0)
    {
        std::cout << "App Initialize failed, exit!" << std::endl;
        return Ret;
    }

    while (!ShadowEngineInterface::G_App->IsQuit())
    {
        ShadowEngineInterface::G_App->Tick(0.0F);
        //Test Code
        //std::cout << "Test" << std::endl;
        //break;
    }

    ShadowEngineInterface::G_App->Finalize();
    delete ShadowEngineInterface::G_App;
    ShadowEngineInterface::G_App = nullptr;

    return 0;
}
