#include "ThirdPartyAdapter/Shader/Public/SlangAdapter.h"

#include <string>
#include <type_traits>
#include <utility>

namespace ShadowEngine
{
    namespace
    {
        class ShaderSourceBlob final : public ISlangBlob
        {
            public:
                explicit ShaderSourceBlob(std::string InSource)
                    : Source(std::move(InSource))
                {
                }

                ISlangUnknown* getInterface(SlangUUID const& Id)
                {
                    if (Id == ISlangUnknown::getTypeGuid() || Id == ISlangBlob::getTypeGuid())
                    {
                        return static_cast<ISlangBlob*>(this);
                    }

                    return nullptr;
                }

                SLANG_IUNKNOWN_ALL

                void const* SLANG_MCALL getBufferPointer() SLANG_OVERRIDE
                {
                    return Source.data();
                }

                size_t SLANG_MCALL getBufferSize() SLANG_OVERRIDE
                {
                    return Source.size();
                }

            private:
                uint32_t m_refCount = 1;
                std::string Source;
        };

        void SetError(std::string* ErrorMessage, const std::string& Message)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = Message;
            }
        }
    }

    bool SlangReflectionApi::Load(void* (*FindSymbol)(void* User, const char* Name), void* User, std::string* ErrorMessage)
    {
        const auto LoadSymbol = [&](const char* Name, auto& Out) -> bool
        {
            Out = reinterpret_cast<std::remove_reference_t<decltype(Out)>>(FindSymbol(User, Name));
            if (Out == nullptr)
            {
                SetError(ErrorMessage, std::string("Slang library does not export ") + Name);
                return false;
            }

            return true;
        };

        return LoadSymbol("spReflectionType_GetKind", GetKind) &&
            LoadSymbol("spReflectionType_GetElementCount", GetElementCount) &&
            LoadSymbol("spReflectionType_GetScalarType", GetScalarType) &&
            LoadSymbol("spReflectionType_GetName", GetTypeName) &&
            LoadSymbol("spReflectionTypeLayout_getKind", GetLayoutKind) &&
            LoadSymbol("spReflectionTypeLayout_GetType", GetType) &&
            LoadSymbol("spReflectionTypeLayout_GetSize", GetSize) &&
            LoadSymbol("spReflectionTypeLayout_GetFieldCount", GetFieldCount) &&
            LoadSymbol("spReflectionTypeLayout_GetFieldByIndex", GetFieldByIndex) &&
            LoadSymbol("spReflectionTypeLayout_GetElementTypeLayout", GetElementTypeLayout) &&
            LoadSymbol("spReflectionTypeLayout_GetElementStride", GetElementStride) &&
            LoadSymbol("spReflectionTypeLayout_GetElementVarLayout", GetElementVarLayout) &&
            LoadSymbol("spReflectionTypeLayout_getContainerVarLayout", GetContainerVarLayout) &&
            LoadSymbol("spReflectionVariable_GetName", GetVariableName) &&
            LoadSymbol("spReflectionVariableLayout_GetVariable", GetVariable) &&
            LoadSymbol("spReflectionVariableLayout_GetTypeLayout", GetVariableTypeLayout) &&
            LoadSymbol("spReflectionVariableLayout_GetOffset", GetOffset) &&
            LoadSymbol("spReflectionVariableLayout_GetSpace", GetSpace) &&
            LoadSymbol("spReflectionParameter_GetBindingIndex", GetBindingIndex) &&
            LoadSymbol("spReflection_getGlobalParamsVarLayout", GetGlobalParams);
    }

    SlangResult SlangAdapter::CreateGlobalSession(void* Function, slang::IGlobalSession** Session)
    {
        const auto Create = reinterpret_cast<SlangResult (*)(SlangInt, slang::IGlobalSession**)>(Function);
        return Create(SLANG_API_VERSION, Session);
    }

    SlangProfileID SlangAdapter::FindProfile(slang::IGlobalSession* Session, const char* Profile)
    {
        return Session->findProfile(Profile);
    }

    SlangResult SlangAdapter::CreateSession(
        slang::IGlobalSession* Session,
        const slang::SessionDesc& Description,
        slang::ISession** Out)
    {
        return Session->createSession(Description, Out);
    }

    slang::IModule* SlangAdapter::LoadModule(slang::ISession* Session, const char* Name, slang::IBlob** Diagnostics)
    {
        return Session->loadModule(Name, Diagnostics);
    }

    Slang::ComPtr<ISlangBlob> SlangAdapter::CreateSourceBlob(std::string Source)
    {
        Slang::ComPtr<ISlangBlob> Blob;
        Blob.attach(new ShaderSourceBlob(std::move(Source)));
        return Blob;
    }

    slang::IModule* SlangAdapter::LoadModuleFromSource(
        slang::ISession* Session,
        const char* Name,
        const char* Path,
        ISlangBlob* Source,
        slang::IBlob** Diagnostics)
    {
        return Session->loadModuleFromSource(Name, Path, Source, Diagnostics);
    }

    SlangResult SlangAdapter::FindEntryPoint(
        slang::IModule* Module,
        const char* Name,
        SlangStage Stage,
        slang::IEntryPoint** EntryPoint,
        slang::IBlob** Diagnostics)
    {
        return Module->findAndCheckEntryPoint(Name, Stage, EntryPoint, Diagnostics);
    }

    SlangResult SlangAdapter::CreateComposite(
        slang::ISession* Session,
        slang::IComponentType* const* Components,
        SlangInt Count,
        slang::IComponentType** Composite,
        slang::IBlob** Diagnostics)
    {
        return Session->createCompositeComponentType(Components, Count, Composite, Diagnostics);
    }

    SlangResult SlangAdapter::Link(
        slang::IComponentType* Composite,
        slang::IComponentType** Linked,
        slang::IBlob** Diagnostics)
    {
        return Composite->link(Linked, Diagnostics);
    }

    SlangResult SlangAdapter::GetEntryPointCode(
        slang::IComponentType* Linked,
        slang::IBlob** Code,
        slang::IBlob** Diagnostics)
    {
        return Linked->getEntryPointCode(0, 0, Code, Diagnostics);
    }

    slang::ProgramLayout* SlangAdapter::GetLayout(slang::IComponentType* Linked)
    {
        return Linked->getLayout();
    }

    const void* SlangAdapter::BlobData(slang::IBlob* Blob)
    {
        return Blob == nullptr ? nullptr : Blob->getBufferPointer();
    }

    size_t SlangAdapter::BlobSize(slang::IBlob* Blob)
    {
        return Blob == nullptr ? 0 : Blob->getBufferSize();
    }
}
