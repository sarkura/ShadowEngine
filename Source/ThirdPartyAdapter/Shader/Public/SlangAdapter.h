#pragma once

#include <slang.h>
#include <slang-com-ptr.h>

#include <string>

namespace ShadowEngine
{
    struct SlangReflectionApi
    {
        SlangTypeKind (*GetKind)(SlangReflectionType*) = nullptr;
        size_t (*GetElementCount)(SlangReflectionType*) = nullptr;
        SlangScalarType (*GetScalarType)(SlangReflectionType*) = nullptr;
        char const* (*GetTypeName)(SlangReflectionType*) = nullptr;
        SlangTypeKind (*GetLayoutKind)(SlangReflectionTypeLayout*) = nullptr;
        SlangReflectionType* (*GetType)(SlangReflectionTypeLayout*) = nullptr;
        size_t (*GetSize)(SlangReflectionTypeLayout*, SlangParameterCategory) = nullptr;
        uint32_t (*GetFieldCount)(SlangReflectionTypeLayout*) = nullptr;
        SlangReflectionVariableLayout* (*GetFieldByIndex)(SlangReflectionTypeLayout*, unsigned) = nullptr;
        SlangReflectionTypeLayout* (*GetElementTypeLayout)(SlangReflectionTypeLayout*) = nullptr;
        size_t (*GetElementStride)(SlangReflectionTypeLayout*, SlangParameterCategory) = nullptr;
        SlangReflectionVariableLayout* (*GetElementVarLayout)(SlangReflectionTypeLayout*) = nullptr;
        SlangReflectionVariableLayout* (*GetContainerVarLayout)(SlangReflectionTypeLayout*) = nullptr;
        char const* (*GetVariableName)(SlangReflectionVariable*) = nullptr;
        SlangReflectionVariable* (*GetVariable)(SlangReflectionVariableLayout*) = nullptr;
        SlangReflectionTypeLayout* (*GetVariableTypeLayout)(SlangReflectionVariableLayout*) = nullptr;
        size_t (*GetOffset)(SlangReflectionVariableLayout*, SlangParameterCategory) = nullptr;
        size_t (*GetSpace)(SlangReflectionVariableLayout*, SlangParameterCategory) = nullptr;
        unsigned (*GetBindingIndex)(SlangReflectionParameter*) = nullptr;
        SlangReflectionVariableLayout* (*GetGlobalParams)(SlangReflection*) = nullptr;

        bool Load(void* (*FindSymbol)(void* User, const char* Name), void* User, std::string* ErrorMessage);
    };

    class SlangAdapter final
    {
        public:
            static SlangResult CreateGlobalSession(void* Function, slang::IGlobalSession** Session);
            static SlangProfileID FindProfile(slang::IGlobalSession* Session, const char* Profile);
            static SlangResult CreateSession(
                slang::IGlobalSession* Session,
                const slang::SessionDesc& Description,
                slang::ISession** Out);
            static slang::IModule* LoadModule(slang::ISession* Session, const char* Name, slang::IBlob** Diagnostics);
            static Slang::ComPtr<ISlangBlob> CreateSourceBlob(std::string Source);
            static slang::IModule* LoadModuleFromSource(
                slang::ISession* Session,
                const char* Name,
                const char* Path,
                ISlangBlob* Source,
                slang::IBlob** Diagnostics);
            static SlangResult FindEntryPoint(
                slang::IModule* Module,
                const char* Name,
                SlangStage Stage,
                slang::IEntryPoint** EntryPoint,
                slang::IBlob** Diagnostics);
            static SlangResult CreateComposite(
                slang::ISession* Session,
                slang::IComponentType* const* Components,
                SlangInt Count,
                slang::IComponentType** Composite,
                slang::IBlob** Diagnostics);
            static SlangResult Link(
                slang::IComponentType* Composite,
                slang::IComponentType** Linked,
                slang::IBlob** Diagnostics);
            static SlangResult GetEntryPointCode(
                slang::IComponentType* Linked,
                slang::IBlob** Code,
                slang::IBlob** Diagnostics);
            static slang::ProgramLayout* GetLayout(slang::IComponentType* Linked);
            static const void* BlobData(slang::IBlob* Blob);
            static size_t BlobSize(slang::IBlob* Blob);
    };
}
