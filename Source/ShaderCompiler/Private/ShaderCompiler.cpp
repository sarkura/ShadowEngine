#include "ShaderCompiler/Public/ShaderCompiler.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/Engine/Public/EngineModules.h"
#include "Framework/Shader/Public/IShaderCompiler.h"
#include "Framework/Shader/Public/ShaderReflection.h"

#include <slang.h>
#include <slang-com-ptr.h>

#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

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
                    return "sm_6_8";
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

        struct ReflectionApi
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

            bool Load(const SharedLibrary& Library, std::string* ErrorMessage)
            {
                const auto LoadSymbol = [&](const char* Name, auto& Out) -> bool
                {
                    Out = reinterpret_cast<std::remove_reference_t<decltype(Out)>>(Library.FindSymbol(Name));
                    if (Out == nullptr)
                    {
                        SetErrorMessage(ErrorMessage, std::string("Slang library does not export ") + Name);
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
        };

        bool IsFloat(const ReflectionApi& Api, SlangReflectionType* Type)
        {
            return Type != nullptr && Api.GetKind(Type) == SLANG_TYPE_KIND_SCALAR &&
                Api.GetScalarType(Type) == SLANG_SCALAR_TYPE_FLOAT32;
        }

        bool IsFloatVector(const ReflectionApi& Api, SlangReflectionType* Type, uint32 Count)
        {
            return Type != nullptr && Api.GetKind(Type) == SLANG_TYPE_KIND_VECTOR &&
                Api.GetElementCount(Type) == Count && Api.GetScalarType(Type) == SLANG_SCALAR_TYPE_FLOAT32;
        }

        bool IsUInt(const ReflectionApi& Api, SlangReflectionType* Type)
        {
            return Type != nullptr && Api.GetKind(Type) == SLANG_TYPE_KIND_SCALAR &&
                Api.GetScalarType(Type) == SLANG_SCALAR_TYPE_UINT32;
        }

        uint32 BindingSlot(
            const ReflectionApi& Api,
            SlangReflectionVariableLayout* Variable,
            SlangParameterCategory Category)
        {
            if (Variable == nullptr)
            {
                return 0;
            }

            const size_t Offset = Api.GetOffset(Variable, Category);
            if (Offset != SLANG_UNKNOWN_SIZE)
            {
                return static_cast<uint32>(Offset);
            }

            const unsigned Index = Api.GetBindingIndex(reinterpret_cast<SlangReflectionParameter*>(Variable));
            return Index == SLANG_UNKNOWN_SIZE ? 0 : Index;
        }

        void CollectBinding(
            const ReflectionApi& Api,
            SlangReflectionVariableLayout* Variable,
            const std::string& ParentType,
            uint32 BaseOffset,
            ShaderReflection& Out)
        {
            if (Variable == nullptr)
            {
                return;
            }

            SlangReflectionTypeLayout* TypeLayout = Api.GetVariableTypeLayout(Variable);
            if (TypeLayout == nullptr)
            {
                return;
            }

            SlangReflectionType* Type = Api.GetType(TypeLayout);
            const SlangTypeKind Kind = Api.GetLayoutKind(TypeLayout);
            SlangReflectionVariable* VariableInfo = Api.GetVariable(Variable);
            const char* RawName = VariableInfo == nullptr ? nullptr : Api.GetVariableName(VariableInfo);
            const std::string Name = RawName == nullptr ? std::string() : std::string(RawName);

            if (Kind == SLANG_TYPE_KIND_PARAMETER_BLOCK)
            {
                CollectBinding(Api, Api.GetElementVarLayout(TypeLayout), ParentType, 0, Out);
                return;
            }

            if (Kind == SLANG_TYPE_KIND_CONSTANT_BUFFER)
            {
                ShaderBinding Binding;
                Binding.Name = Name.empty() ? "Constants" : Name;
                Binding.Kind = EShaderBindingKind::ConstantBuffer;
                SlangReflectionVariableLayout* Container = Api.GetContainerVarLayout(TypeLayout);
                Binding.Slot = BindingSlot(
                    Api,
                    Container != nullptr ? Container : Variable,
                    SLANG_PARAMETER_CATEGORY_CONSTANT_BUFFER);
                const size_t Space = Api.GetSpace(Variable, SLANG_PARAMETER_CATEGORY_CONSTANT_BUFFER);
                Binding.Space = Space == SLANG_UNKNOWN_SIZE ? 0 : static_cast<uint32>(Space);
                if (SlangReflectionTypeLayout* ElementType = Api.GetElementTypeLayout(TypeLayout))
                {
                    const size_t Size = Api.GetSize(ElementType, SLANG_PARAMETER_CATEGORY_UNIFORM);
                    Binding.Size = Size == SLANG_UNKNOWN_SIZE ? 0 : static_cast<uint32>(Size);
                }
                Out.Bindings.push_back(Binding);
                CollectBinding(Api, Api.GetElementVarLayout(TypeLayout), {}, 0, Out);
                return;
            }

            if (Kind == SLANG_TYPE_KIND_STRUCT)
            {
                const char* TypeName = Type == nullptr ? nullptr : Api.GetTypeName(Type);
                const std::string NextParent = TypeName == nullptr ? Name : std::string(TypeName);
                const size_t FieldOffset = Api.GetOffset(Variable, SLANG_PARAMETER_CATEGORY_UNIFORM);
                const uint32 NextBase = FieldOffset == SLANG_UNKNOWN_SIZE ? BaseOffset : BaseOffset + static_cast<uint32>(FieldOffset);
                const uint32_t Count = Api.GetFieldCount(TypeLayout);
                for (uint32_t Index = 0; Index < Count; ++Index)
                {
                    CollectBinding(Api, Api.GetFieldByIndex(TypeLayout, Index), NextParent, NextBase, Out);
                }
                return;
            }

            if (Kind == SLANG_TYPE_KIND_ARRAY)
            {
                SlangReflectionTypeLayout* ElementLayout = Api.GetElementTypeLayout(TypeLayout);
                SlangReflectionType* ElementType = ElementLayout == nullptr ? nullptr : Api.GetType(ElementLayout);
                const char* ElementTypeName = ElementType == nullptr ? nullptr : Api.GetTypeName(ElementType);
                const size_t Count = Type == nullptr ? 0 : Api.GetElementCount(Type);
                const size_t Stride = Api.GetElementStride(TypeLayout, SLANG_PARAMETER_CATEGORY_UNIFORM);
                const size_t ArrayOffset = Api.GetOffset(Variable, SLANG_PARAMETER_CATEGORY_UNIFORM);
                if (ElementLayout == nullptr || ElementTypeName == nullptr ||
                    std::string(ElementTypeName) != "ForwardPointLight" || Count == 0 ||
                    Count == SLANG_UNKNOWN_SIZE || Count == SLANG_UNBOUNDED_SIZE ||
                    Stride == SLANG_UNKNOWN_SIZE || Stride == SLANG_UNBOUNDED_SIZE ||
                    ArrayOffset == SLANG_UNKNOWN_SIZE)
                {
                    return;
                }

                const uint32 ArrayBase = BaseOffset + static_cast<uint32>(ArrayOffset);
                const uint32_t FieldCount = Api.GetFieldCount(ElementLayout);
                for (size_t Index = 0; Index < Count; ++Index)
                {
                    const std::string ElementParent = std::string("ForwardPointLight[") + std::to_string(Index) + "]";
                    const uint32 ElementBase = ArrayBase + static_cast<uint32>(Index * Stride);
                    for (uint32_t FieldIndex = 0; FieldIndex < FieldCount; ++FieldIndex)
                    {
                        CollectBinding(Api, Api.GetFieldByIndex(ElementLayout, FieldIndex), ElementParent, ElementBase, Out);
                    }
                }
                return;
            }

            const size_t FieldOffset = Api.GetOffset(Variable, SLANG_PARAMETER_CATEGORY_UNIFORM);
            if (FieldOffset == SLANG_UNKNOWN_SIZE)
            {
                return;
            }

            const uint32 Offset = BaseOffset + static_cast<uint32>(FieldOffset);
            const bool bPointLight = ParentType == "ForwardPointLight" || ParentType.rfind("ForwardPointLight[", 0) == 0;
            const bool bLight = ParentType == "ForwardDirectLight" || ParentType == "ForwardSkyLight" ||
                ParentType == "ForwardShadow" || bPointLight;
            if (bLight && IsFloatVector(Api, Type, 4))
            {
                ShaderBinding Binding;
                Binding.Name = ParentType + "." + Name;
                Binding.Kind = EShaderBindingKind::Uniform;
                Binding.Offset = Offset;
                Binding.Size = 16;
                Out.Bindings.push_back(std::move(Binding));
                return;
            }

            if (ParentType == "MaterialSurfaceData" && Name.rfind("Padding", 0) != 0)
            {
                ShaderBinding Binding;
                Binding.Name = Name;
                Binding.Offset = Offset;
                if (IsFloat(Api, Type))
                {
                    Binding.Kind = EShaderBindingKind::Float;
                    Binding.Size = 4;
                }
                else if (IsFloatVector(Api, Type, 2))
                {
                    Binding.Kind = EShaderBindingKind::Float2;
                    Binding.Size = 8;
                }
                else if (IsFloatVector(Api, Type, 3))
                {
                    Binding.Kind = EShaderBindingKind::Float3;
                    Binding.Size = 12;
                }
                else if (IsFloatVector(Api, Type, 4))
                {
                    Binding.Kind = EShaderBindingKind::Float4;
                    Binding.Size = 16;
                }
                else
                {
                    return;
                }

                Out.Bindings.push_back(std::move(Binding));
                return;
            }

            const bool bDescriptorStruct =
                ParentType == "MaterialDescriptorIndices" || ParentType == "ImageBasedLightIndices" ||
                ParentType == "ForwardShadowIndices";
            if (!IsUInt(Api, Type) || !bDescriptorStruct)
            {
                return;
            }

            if (Name.rfind("Padding", 0) == 0)
            {
                return;
            }

            ShaderBinding Binding;
            Binding.Offset = Offset;
            Binding.Size = 4;
            Binding.Name = Name;
            if (ParentType == "ImageBasedLightIndices" || ParentType == "ForwardShadowIndices")
            {
                Binding.Kind = EShaderBindingKind::ResourceIndex;
            }
            else if (Name == "Sampler")
            {
                Binding.Kind = EShaderBindingKind::Sampler;
            }
            else
            {
                Binding.Kind = EShaderBindingKind::Texture2D;
            }

            Out.Bindings.push_back(std::move(Binding));
        }

        void CollectReflection(const ReflectionApi& Api, slang::ProgramLayout* Layout, ShaderReflection& Out)
        {
            Out.Bindings.clear();
            if (Layout == nullptr)
            {
                return;
            }

            CollectBinding(Api, Api.GetGlobalParams(reinterpret_cast<SlangReflection*>(Layout)), {}, 0, Out);
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

                    if (!ReflectionApi.Load(Library, ErrorMessage))
                    {
                        GlobalSession = nullptr;
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
                    ShaderReflection& Reflection,
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
                    slang::IModule* Module = nullptr;
                    if (Request.ImplementationPath.empty() && Request.LightPath.empty())
                    {
                        Module = Session->loadModule(ModuleName.c_str(), Diagnostics.writeRef());
                    }
                    else
                    {
                        const std::filesystem::path Fragment = std::filesystem::absolute(Request.SourcePath);
                        std::string Source;
                        std::string ModuleStem;
                        if (!Request.ScalarFields.empty())
                        {
                            Source += "#define MATERIAL_SURFACE_DEFINED\n";
                            Source += "struct MaterialSurfaceData\n{\n";
                            for (const ShaderScalarField& Field : Request.ScalarFields)
                            {
                                const char* TypeName = nullptr;
                                if (Field.Components == 1)
                                {
                                    TypeName = "float";
                                }
                                else if (Field.Components == 2)
                                {
                                    TypeName = "float2";
                                }
                                else if (Field.Components == 3)
                                {
                                    TypeName = "float3";
                                }
                                else if (Field.Components == 4)
                                {
                                    TypeName = "float4";
                                }
                                else
                                {
                                    SetErrorMessage(ErrorMessage, "Material parameter '" + Field.Name + "' has an unsupported component count");
                                    return false;
                                }

                                Source += "    ";
                                Source += TypeName;
                                Source += " ";
                                Source += Field.Name;
                                Source += ";\n";
                            }
                            Source += "};\n";
                        }
                        if (!Request.DescriptorFields.empty())
                        {
                            constexpr size_t DescriptorSlotCount = 8;
                            if (Request.DescriptorFields.size() + 1 > DescriptorSlotCount)
                            {
                                SetErrorMessage(ErrorMessage, "A material can declare at most 7 textures");
                                return false;
                            }

                            Source += "#define MATERIAL_DESCRIPTOR_INDICES_DEFINED\n";
                            Source += "struct MaterialDescriptorIndices\n{\n";
                            for (const std::string& Field : Request.DescriptorFields)
                            {
                                Source += "    uint " + Field + ";\n";
                            }
                            Source += "    uint Sampler;\n";
                            for (size_t Index = Request.DescriptorFields.size() + 1; Index < DescriptorSlotCount; ++Index)
                            {
                                Source += "    uint Padding" + std::to_string(Index) + ";\n";
                            }
                            Source += "};\n";
                        }
                        if (!Request.ImplementationPath.empty())
                        {
                            const std::filesystem::path Implementation =
                                std::filesystem::absolute(Request.ImplementationPath);
                            Source += "#include \"" + Implementation.generic_string() + "\"\n";
                            ModuleStem += Implementation.stem().string() + "+";
                        }
                        if (!Request.LightPath.empty())
                        {
                            const std::filesystem::path Light = std::filesystem::absolute(Request.LightPath);
                            Source += "#include \"" + Light.generic_string() + "\"\n";
                            ModuleStem += Light.stem().string() + "+";
                        }
                        Source += "#include \"" + Fragment.generic_string() + "\"\n";
                        ModuleStem += Fragment.filename().string();
                        Slang::ComPtr<ISlangBlob> SourceBlob;
                        SourceBlob.attach(new ShaderSourceBlob(Source));

                        const std::string ModulePath = (Fragment.parent_path() / ModuleStem).generic_string();
                        Module = Session->loadModuleFromSource(
                            ModuleName.c_str(),
                            ModulePath.c_str(),
                            SourceBlob,
                            Diagnostics.writeRef());
                    }
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
                    CollectReflection(ReflectionApi, Linked->getLayout(), Reflection);
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
                ReflectionApi ReflectionApi;
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
