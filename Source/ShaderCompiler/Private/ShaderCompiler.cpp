#include "ShaderCompiler/Public/ShaderCompiler.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/Engine/Public/EngineModules.h"
#include "Framework/Shader/Public/IShaderCompiler.h"

#include <slang.h>
#include <slang-com-ptr.h>

#include <cstring>
#include <memory>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace ShadowEngine
{
    namespace
    {
        using CreateGlobalSessionFunction = SlangResult (*)(SlangInt, slang::IGlobalSession**);

        constexpr char CreateGlobalSessionSymbol[] = "slang_createGlobalSession";

        class SharedLibrary final
        {
            public:
                ~SharedLibrary()
                {
                    Unload();
                }

                bool Load(std::string* ErrorMessage)
                {
#ifdef _WIN32
                    Handle = LoadLibraryW(L"slang-compiler.dll");
#else
                    Handle = dlopen("libslang-compiler.so", RTLD_NOW | RTLD_LOCAL);
#endif
                    if (Handle == nullptr)
                    {
                        SetErrorMessage(
                            ErrorMessage,
                            "Unable to load the Slang compiler library, it must be next to the executable");
                        return false;
                    }

                    return true;
                }

                void Unload()
                {
                    if (Handle == nullptr)
                    {
                        return;
                    }

#ifdef _WIN32
                    FreeLibrary(static_cast<HMODULE>(Handle));
#else
                    dlclose(Handle);
#endif
                    Handle = nullptr;
                }

                [[nodiscard]] void* FindSymbol(const char* Name) const
                {
#ifdef _WIN32
                    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(Handle), Name));
#else
                    return dlsym(Handle, Name);
#endif
                }

            private:
                void* Handle = nullptr;
        };

        SlangCompileTarget ToSlangTarget(ERHIShaderFormat Format)
        {
            switch (Format)
            {
                case ERHIShaderFormat::DXBC:
                    return SLANG_DXBC;
                case ERHIShaderFormat::DXIL:
                    return SLANG_DXIL;
                case ERHIShaderFormat::SPIRV:
                    return SLANG_SPIRV;
            }

            return SLANG_TARGET_UNKNOWN;
        }

        const char* ToSlangProfile(ERHIShaderFormat Format)
        {
            switch (Format)
            {
                case ERHIShaderFormat::DXBC:
                    return "sm_5_1";
                case ERHIShaderFormat::DXIL:
                    return "sm_6_0";
                case ERHIShaderFormat::SPIRV:
                    return "spirv_1_5";
            }

            return "";
        }

        SlangStage ToSlangStage(ERHIShaderStage Stage)
        {
            switch (Stage)
            {
                case ERHIShaderStage::Vertex:
                    return SLANG_STAGE_VERTEX;
                case ERHIShaderStage::Pixel:
                    return SLANG_STAGE_FRAGMENT;
            }

            return SLANG_STAGE_NONE;
        }

        std::string ToString(slang::IBlob* Blob)
        {
            if (Blob == nullptr)
            {
                return {};
            }

            return {
                static_cast<const char*>(Blob->getBufferPointer()),
                Blob->getBufferSize()};
        }

        class SlangShaderCompiler final : public IShaderCompiler
        {
            public:
                ~SlangShaderCompiler() override
                {
                    Finalize();
                }

                bool Initialize(std::string* ErrorMessage) override
                {
                    if (!Library.Load(ErrorMessage))
                    {
                        return false;
                    }

                    const auto CreateGlobalSession = reinterpret_cast<CreateGlobalSessionFunction>(
                        Library.FindSymbol(CreateGlobalSessionSymbol));
                    if (CreateGlobalSession == nullptr)
                    {
                        SetErrorMessage(
                            ErrorMessage,
                            std::string("Slang library does not export ") + CreateGlobalSessionSymbol);
                        Library.Unload();
                        return false;
                    }

                    if (SLANG_FAILED(CreateGlobalSession(SLANG_API_VERSION, GlobalSession.writeRef())))
                    {
                        SetErrorMessage(ErrorMessage, "Unable to create the Slang global session");
                        Library.Unload();
                        return false;
                    }

                    Log::Info("Slang shader compiler loaded");
                    return true;
                }

                void Finalize() override
                {
                    GlobalSession = nullptr;
                    Library.Unload();
                }

                bool Compile(
                    const ShaderCompileRequest& Request,
                    ShaderBytecode& Bytecode,
                    std::string* ErrorMessage) override
                {
                    if (GlobalSession == nullptr)
                    {
                        SetErrorMessage(ErrorMessage, "Slang shader compiler is not initialized");
                        return false;
                    }

                    const std::string Label =
                        Request.SourcePath.generic_string() + ":" + Request.EntryPoint;

                    slang::TargetDesc Target{};
                    Target.format = ToSlangTarget(Request.Format);
                    Target.profile = GlobalSession->findProfile(ToSlangProfile(Request.Format));

                    const std::string SearchPath = Request.SourcePath.parent_path().string();
                    const char* SearchPaths[] = {SearchPath.c_str()};

                    slang::SessionDesc SessionDesc{};
                    SessionDesc.targets = &Target;
                    SessionDesc.targetCount = 1;
                    // GLM writes column-major matrices. Slang's default layout is row-major.
                    SessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
                    SessionDesc.searchPaths = SearchPaths;
                    SessionDesc.searchPathCount = 1;

                    Slang::ComPtr<slang::ISession> Session;
                    if (SLANG_FAILED(GlobalSession->createSession(SessionDesc, Session.writeRef())))
                    {
                        SetErrorMessage(ErrorMessage, "Unable to create a Slang session for " + Label);
                        return false;
                    }

                    Slang::ComPtr<slang::IBlob> Diagnostics;
                    const std::string ModuleName = Request.SourcePath.stem().string();
                    slang::IModule* Module = Session->loadModule(ModuleName.c_str(), Diagnostics.writeRef());
                    if (Module == nullptr)
                    {
                        return Fail(ErrorMessage, "Unable to load shader module " + Label, Diagnostics);
                    }

                    Slang::ComPtr<slang::IEntryPoint> EntryPoint;
                    if (SLANG_FAILED(Module->findAndCheckEntryPoint(
                            Request.EntryPoint.c_str(),
                            ToSlangStage(Request.Stage),
                            EntryPoint.writeRef(),
                            Diagnostics.writeRef())))
                    {
                        return Fail(ErrorMessage, "Unable to find entry point " + Label, Diagnostics);
                    }

                    slang::IComponentType* Components[] = {Module, EntryPoint.get()};
                    Slang::ComPtr<slang::IComponentType> Composite;
                    if (SLANG_FAILED(Session->createCompositeComponentType(
                            Components,
                            2,
                            Composite.writeRef(),
                            Diagnostics.writeRef())))
                    {
                        return Fail(ErrorMessage, "Unable to compose shader " + Label, Diagnostics);
                    }

                    Slang::ComPtr<slang::IComponentType> Linked;
                    if (SLANG_FAILED(Composite->link(Linked.writeRef(), Diagnostics.writeRef())))
                    {
                        return Fail(ErrorMessage, "Unable to link shader " + Label, Diagnostics);
                    }

                    Slang::ComPtr<slang::IBlob> Code;
                    if (SLANG_FAILED(Linked->getEntryPointCode(0, 0, Code.writeRef(), Diagnostics.writeRef())))
                    {
                        return Fail(ErrorMessage, "Unable to generate code for " + Label, Diagnostics);
                    }

                    if (Diagnostics != nullptr && Diagnostics->getBufferSize() > 0)
                    {
                        Log::Warning("Slang diagnostics for {}:\n{}", Label, ToString(Diagnostics));
                    }

                    const auto* Data = static_cast<const uint8*>(Code->getBufferPointer());
                    Bytecode.Stage = Request.Stage;
                    Bytecode.EntryPoint = Request.EntryPoint;
                    Bytecode.Data.assign(Data, Data + Code->getBufferSize());
                    return true;
                }

            private:
                static bool Fail(
                    std::string* ErrorMessage,
                    std::string Message,
                    const Slang::ComPtr<slang::IBlob>& Diagnostics)
                {
                    const std::string Details = ToString(Diagnostics);
                    if (!Details.empty())
                    {
                        Message += "\n" + Details;
                    }

                    SetErrorMessage(ErrorMessage, std::move(Message));
                    return false;
                }

                SharedLibrary Library;
                Slang::ComPtr<slang::IGlobalSession> GlobalSession;
        };
    }

    void RegisterSlangShaderCompiler()
    {
        EngineModules::Get().RegisterShaderCompiler([]
        {
            return std::make_unique<SlangShaderCompiler>();
        });
    }
}
