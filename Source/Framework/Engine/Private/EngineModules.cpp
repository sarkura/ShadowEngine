#include "Framework/Engine/Public/EngineModules.h"

#include <utility>

namespace ShadowEngine
{
    EngineModules& EngineModules::Get()
    {
        static EngineModules Instance;
        return Instance;
    }

    void EngineModules::RegisterRHI(std::string Name, RHIDeviceFactory Factory)
    {
        RHIFactories.insert_or_assign(std::move(Name), std::move(Factory));
    }

    void EngineModules::RegisterShaderCompiler(ShaderCompilerFactory Factory)
    {
        CompilerFactory = std::move(Factory);
    }

    std::unique_ptr<RHIDevice> EngineModules::CreateRHIDevice(std::string_view Name) const
    {
        const auto Iterator = RHIFactories.find(Name);
        if (Iterator == RHIFactories.end())
        {
            return nullptr;
        }

        return Iterator->second();
    }

    std::unique_ptr<IShaderCompiler> EngineModules::CreateShaderCompiler() const
    {
        if (!CompilerFactory)
        {
            return nullptr;
        }

        return CompilerFactory();
    }
}
