#include "Framework/Asset/Public/AssetManager.h"

#include "Framework/Asset/Public/MaterialAsset.h"
#include "Framework/Asset/Public/TextureAsset.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/Config/Public/ConfigManager.h"
#include "Framework/Config/Public/DescriptionParser.h"
#include "Framework/Shader/Public/ShaderManager.h"
#include "Framework/Material/Public/Material.h"
#include "Framework/Material/Public/MaterialInstance.h"
#include "Framework/Material/Public/MaterialParameter.h"
#include "CookedAsset/Public/CookedAsset.h"
#include "ThirdPartyAdapter/Texture/Public/STBAdapter.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string_view>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace ShadowEngine
{
    namespace
    {
        std::vector<ShaderScalarField> ScalarFieldsFor(const MaterialRendererBinding& Binding)
        {
            std::vector<ShaderScalarField> Fields;
            Fields.reserve(Binding.ShaderScalars.size());
            for (const MaterialScalarParameter& Scalar : Binding.ShaderScalars)
            {
                ShaderScalarField Field;
                Field.Name = Scalar.Name;
                Field.Components = Scalar.Components;
                Fields.push_back(std::move(Field));
            }

            return Fields;
        }


        struct MeshBlock
        {
            uint32 Slot = 0;
            std::string MaterialPath;
            MaterialInstanceHandle Material;
            bool bRequireTexCoord = false;
            std::vector<MeshVertex> Vertices;
            std::vector<uint32> Indices;
        };

        MeshBlock* FindBlock(std::vector<MeshBlock>& Blocks, uint32 Slot)
        {
            for (MeshBlock& Block : Blocks)
            {
                if (Block.Slot == Slot)
                {
                    return &Block;
                }
            }

            return nullptr;
        }


        bool WriteMeshMaterial(
            const std::filesystem::path& MeshPath,
            uint32 Slot,
            const std::string& OldMaterial,
            const std::string& NewMaterial,
            std::string* ErrorMessage)
        {
            std::ifstream Input(MeshPath, std::ios::binary);
            if (!Input)
            {
                SetErrorMessage(ErrorMessage, "Unable to open mesh asset: " + MeshPath.generic_string());
                return false;
            }

            std::string Json{
                std::istreambuf_iterator<char>(Input),
                std::istreambuf_iterator<char>()};
            Input.close();

            const std::string SlotText = std::to_string(Slot);
            size_t Search = 0;
            bool bReplaced = false;
            while (Search < Json.size())
            {
                const size_t SlotPos = Json.find("\"slot\"", Search);
                if (SlotPos == std::string::npos)
                {
                    break;
                }

                const size_t Colon = Json.find(':', SlotPos + 6);
                if (Colon == std::string::npos)
                {
                    break;
                }

                size_t Number = Colon + 1;
                while (Number < Json.size() && std::isspace(static_cast<unsigned char>(Json[Number])) != 0)
                {
                    ++Number;
                }

                const size_t NumberEnd = Number + SlotText.size();
                if (Json.compare(Number, SlotText.size(), SlotText) != 0 ||
                    (NumberEnd < Json.size() && std::isdigit(static_cast<unsigned char>(Json[NumberEnd])) != 0))
                {
                    Search = Number + 1;
                    continue;
                }

                const size_t ObjectStart = Json.rfind('{', SlotPos);
                const size_t ObjectEnd = Json.find('}', NumberEnd);
                if (ObjectStart == std::string::npos || ObjectEnd == std::string::npos)
                {
                    break;
                }

                const size_t MaterialKey = Json.find("\"material\"", ObjectStart);
                const size_t ValueColon = MaterialKey < ObjectEnd ? Json.find(':', MaterialKey + 10) : std::string::npos;
                const size_t OpenQuote = ValueColon < ObjectEnd ? Json.find('"', ValueColon + 1) : std::string::npos;
                const size_t CloseQuote = OpenQuote < ObjectEnd ? Json.find('"', OpenQuote + 1) : std::string::npos;
                if (MaterialKey > ObjectEnd || CloseQuote > ObjectEnd)
                {
                    Search = ObjectEnd + 1;
                    continue;
                }

                const std::string Current = Json.substr(OpenQuote + 1, CloseQuote - OpenQuote - 1);
                if (Current != OldMaterial)
                {
                    Search = CloseQuote + 1;
                    continue;
                }

                Json.replace(OpenQuote + 1, Current.size(), NewMaterial);
                bReplaced = true;
                break;
            }

            if (!bReplaced)
            {
                SetErrorMessage(ErrorMessage, "Mesh asset material slot was not written: " + MeshPath.generic_string());
                return false;
            }

            std::ofstream Output(MeshPath, std::ios::binary | std::ios::trunc);
            if (!Output)
            {
                SetErrorMessage(ErrorMessage, "Unable to write mesh asset: " + MeshPath.generic_string());
                return false;
            }

            Output.write(Json.data(), static_cast<std::streamsize>(Json.size()));
            if (!Output)
            {
                SetErrorMessage(ErrorMessage, "Unable to write mesh asset: " + MeshPath.generic_string());
                return false;
            }

            return true;
        }

    }

    AssetManager::~AssetManager()
    {
        Finalize();
    }

    void AssetManager::SetShaderManager(ShaderManager& InShaders)
    {
        Shaders = &InShaders;
    }

    void AssetManager::Finalize()
    {
        MaterialInstances.clear();
        MaterialInstancePaths.clear();
        Materials.clear();
        SharedMaterialKeys.clear();
        Textures.clear();
        Meshes.clear();
        LinearSampler = {};
        Registry.Clear();
    }

    MeshHandle AssetManager::LoadMesh(
        const std::filesystem::path& Path,
        std::string* ErrorMessage)
    {
        if (Path.empty())
        {
            SetErrorMessage(ErrorMessage, "Mesh path is empty");
            return {};
        }

        if (const MeshAsset* Existing = Registry.FindMesh(Path))
        {
            for (uint32 Index = 0; Index < Meshes.size(); ++Index)
            {
                if (Meshes[Index] == Existing)
                {
                    MeshHandle Handle;
                    Handle.Index = Index;
                    return Handle;
                }
            }

            SetErrorMessage(ErrorMessage, "Mesh is registered without a handle: " + Path.generic_string());
            return {};
        }

        const std::string DescriptionPath = Path.generic_string();
        MeshDescription Description;
        if (!DescriptionParser::LoadMeshDescription(Path, Description, ErrorMessage))
        {
            return {};
        }

        std::filesystem::path SourceFile = Description.Source;
        std::vector<MeshBlock> Blocks;
        Blocks.reserve(Description.Materials.size());
        for (const MeshMaterialBinding& Binding : Description.Materials)
        {
            MeshBlock Block;
            Block.Slot = Binding.Slot;
            Block.MaterialPath = Binding.Material;
            Blocks.push_back(std::move(Block));
        }

        for (MeshBlock& Block : Blocks)
        {
            std::string MaterialError;
            Block.Material = LoadMaterialInstance(Block.MaterialPath, &MaterialError);
            if (!Block.Material.IsValid())
            {
                const std::string DefaultMaterialPath = std::filesystem::path(
                    ConfigManager::Get().GetRHISetting().DefaultMaterial).generic_string();
                if (DefaultMaterialPath.empty())
                {
                    SetErrorMessage(ErrorMessage, "DefaultMaterial is not configured");
                    return {};
                }

                const std::string Requested = std::filesystem::path(Block.MaterialPath).generic_string();
                if (Requested == DefaultMaterialPath)
                {
                    SetErrorMessage(ErrorMessage, MaterialError);
                    return {};
                }

                Log::Warning(
                    "Mesh {} slot {} material {} failed: {}. Using {}",
                    DescriptionPath,
                    Block.Slot,
                    Requested,
                    MaterialError,
                    DefaultMaterialPath);
                Block.Material = LoadMaterialInstance(DefaultMaterialPath, ErrorMessage);
                if (!Block.Material.IsValid())
                {
                    return {};
                }

                if (!WriteMeshMaterial(Path, Block.Slot, Block.MaterialPath, DefaultMaterialPath, ErrorMessage))
                {
                    return {};
                }

                Log::Info(
                    "Updated mesh asset {} slot {} to {}",
                    DescriptionPath,
                    Block.Slot,
                    DefaultMaterialPath);
            }

            const MaterialInstance* Instance = ResolveMaterialInstance(Block.Material);
            Block.bRequireTexCoord = Instance != nullptr && Instance->HasTextures();
        }

        const std::string SourcePath = SourceFile.generic_string();
        CookedMesh Cooked;
        if (!LoadCookedMesh(CookedMeshPath(SourceFile), Cooked, ErrorMessage))
        {
            return {};
        }

        for (CookedMeshSection& CookedSection : Cooked.Sections)
        {
            MeshBlock* Block = FindBlock(Blocks, CookedSection.Slot);
            if (Block == nullptr)
            {
                SetErrorMessage(
                    ErrorMessage,
                    "Mesh primitive slot " + std::to_string(CookedSection.Slot) + " has no material: " + SourcePath);
                return {};
            }

            if (Block->bRequireTexCoord && !CookedSection.bHasTexCoords)
            {
                SetErrorMessage(ErrorMessage, "Mesh is missing texture coordinates: " + SourcePath);
                return {};
            }

            Block->Vertices = std::move(CookedSection.Vertices);
            Block->Indices = std::move(CookedSection.Indices);
        }

        std::vector<MeshSection> Sections;
        Sections.reserve(Blocks.size());
        size_t VertexCount = 0;
        size_t IndexCount = 0;
        for (MeshBlock& Block : Blocks)
        {
            if (Block.Vertices.empty() || Block.Indices.empty())
            {
                SetErrorMessage(
                    ErrorMessage,
                    "Mesh material slot " + std::to_string(Block.Slot) + " has no triangles: " + SourcePath);
                return {};
            }

            VertexCount += Block.Vertices.size();
            IndexCount += Block.Indices.size();
            MeshSection Section;
            Section.Material = Block.Material;
            Section.Vertices = std::move(Block.Vertices);
            Section.Indices = std::move(Block.Indices);
            Sections.push_back(std::move(Section));
        }

        auto Mesh = std::make_unique<MeshAsset>(Path, std::move(Sections));
        const MeshAsset* Loaded = Mesh.get();
        if (!Registry.Register(std::move(Mesh)))
        {
            SetErrorMessage(ErrorMessage, "Unable to register mesh: " + DescriptionPath);
            return {};
        }

        Meshes.push_back(Loaded);
        Log::Info(
            "Loaded mesh {} from {} ({} sections, {} vertices, {} indices)",
            DescriptionPath,
            SourcePath,
            Loaded->GetSections().size(),
            VertexCount,
            IndexCount);
        MeshHandle Handle;
        Handle.Index = static_cast<uint32>(Meshes.size() - 1);
        return Handle;
    }

    namespace
    {
        bool MergeMaterialBinding(
            MaterialRendererBinding& Base,
            const MaterialRendererBinding& Child,
            const std::string& FilePath,
            std::string* ErrorMessage)
        {
            if (Child.bDefinesShader)
            {
                if (Base.bDefinesShader)
                {
                    SetErrorMessage(
                        ErrorMessage,
                        Child.Renderer + " instance cannot replace shader paths: " + FilePath);
                    return false;
                }

                Base = Child;
                return true;
            }

            if (!Base.bDefinesShader)
            {
                SetErrorMessage(ErrorMessage, Child.Renderer + " is missing shader paths: " + FilePath);
                return false;
            }

            if (Child.bHasSurface)
            {
                Base.Surface = Child.Surface;
                Base.bHasSurface = true;
            }
            if (Child.bHasTwoSided)
            {
                Base.bTwoSided = Child.bTwoSided;
                Base.bHasTwoSided = true;
            }

            for (const MaterialScalarParameter& Scalar : Child.Scalars)
            {
                const MaterialScalarParameter* Schema = nullptr;
                for (const MaterialScalarParameter& Defined : Base.ShaderScalars)
                {
                    if (Defined.Name == Scalar.Name)
                    {
                        Schema = &Defined;
                        break;
                    }
                }
                if (Schema == nullptr)
                {
                    SetErrorMessage(
                        ErrorMessage,
                        Scalar.Name + " is not a parameter of " + Base.Renderer + ": " + FilePath);
                    return false;
                }
                if (Schema->Components != Scalar.Components)
                {
                    SetErrorMessage(
                        ErrorMessage,
                        Scalar.Name + " type does not match " + Base.Renderer + ": " + FilePath);
                    return false;
                }

                bool bFound = false;
                for (MaterialScalarParameter& Existing : Base.Scalars)
                {
                    if (Existing.Name == Scalar.Name)
                    {
                        Existing = Scalar;
                        bFound = true;
                        break;
                    }
                }
                if (!bFound)
                {
                    Base.Scalars.push_back(Scalar);
                }
            }
            for (const auto& Texture : Child.Textures)
            {
                bool bDeclared = false;
                for (const auto& Defined : Base.ShaderTextures)
                {
                    if (Defined.first == Texture.first)
                    {
                        bDeclared = true;
                        break;
                    }
                }
                if (!bDeclared)
                {
                    SetErrorMessage(
                        ErrorMessage,
                        Texture.first + " is not a texture parameter of " + Base.Renderer + ": " + FilePath);
                    return false;
                }

                bool bFound = false;
                for (auto& Existing : Base.Textures)
                {
                    if (Existing.first == Texture.first)
                    {
                        Existing.second = Texture.second;
                        bFound = true;
                        break;
                    }
                }
                if (!bFound)
                {
                    Base.Textures.push_back(Texture);
                }
            }
            if (!Child.Scalars.empty() || !Child.Textures.empty())
            {
                Base.bHasParameters = true;
            }
            return true;
        }

        bool ResolveMaterialDescription(
            const std::filesystem::path& Path,
            MaterialDescription& Out,
            std::vector<std::string>& Stack,
            std::string* ErrorMessage)
        {
            const std::string Key = Path.generic_string();
            for (const std::string& Seen : Stack)
            {
                if (Seen == Key)
                {
                    SetErrorMessage(ErrorMessage, "Material parent cycle: " + Key);
                    return false;
                }
            }

            Stack.push_back(Key);
            struct StackScope
            {
                std::vector<std::string>& Stack;
                ~StackScope()
                {
                    Stack.pop_back();
                }
            } Scope{Stack};

            MaterialDescription Description;
            if (!DescriptionParser::LoadMaterialDescription(Path, Description, ErrorMessage))
            {
                return false;
            }

            MaterialDescription Resolved;
            Resolved.Bindings.reserve(Description.Bindings.size());
            for (const MaterialRendererBinding& Binding : Description.Bindings)
            {
                if (Binding.Parent.empty())
                {
                    if (!Binding.bDefinesShader)
                    {
                        SetErrorMessage(ErrorMessage, Binding.Renderer + " is missing shader paths: " + Key);
                        return false;
                    }

                    Resolved.Bindings.push_back(Binding);
                    continue;
                }

                MaterialDescription Parent;
                if (!ResolveMaterialDescription(Binding.Parent, Parent, Stack, ErrorMessage))
                {
                    return false;
                }

                const MaterialRendererBinding* Source = nullptr;
                for (const MaterialRendererBinding& ParentBinding : Parent.Bindings)
                {
                    if (ParentBinding.Renderer == Binding.Renderer)
                    {
                        Source = &ParentBinding;
                        break;
                    }
                }
                if (Source == nullptr)
                {
                    SetErrorMessage(
                        ErrorMessage,
                        Binding.Renderer + " is missing from parent " + Binding.Parent + ": " + Key);
                    return false;
                }

                MaterialRendererBinding Merged = *Source;
                if (!MergeMaterialBinding(Merged, Binding, Key, ErrorMessage))
                {
                    return false;
                }

                Resolved.Bindings.push_back(std::move(Merged));
            }

            if (Resolved.Bindings.empty())
            {
                SetErrorMessage(ErrorMessage, "Material description has no renderer binding: " + Key);
                return false;
            }

            Out = std::move(Resolved);
            return true;
        }

        bool ExpandPrincipledSurface(MaterialDescription& Description, const std::string& FilePath, std::string* ErrorMessage)
        {
            std::vector<MaterialRendererBinding> Expanded;
            Expanded.reserve(Description.Bindings.size() * 2);
            for (const MaterialRendererBinding& Binding : Description.Bindings)
            {
                if (Binding.Renderer == "BlinnPhong" || Binding.Renderer == "Debug")
                {
                    MaterialRendererBinding Mapped = Binding;
                    Mapped.Renderer = Binding.Renderer == "BlinnPhong" ? "BlinnPhongRenderer" : "DebugRenderer";
                    Expanded.push_back(std::move(Mapped));
                    continue;
                }

                if (Binding.Renderer != "Principled")
                {
                    Expanded.push_back(Binding);
                    continue;
                }

                if (!Binding.PixelShader.empty() || !Binding.LightShader.empty())
                {
                    SetErrorMessage(ErrorMessage, "Principled does not store a fragment or light shader: " + FilePath);
                    return false;
                }

                MaterialRendererBinding Forward = Binding;
                Forward.Renderer = "ForwardRenderer";
                MaterialRendererBinding Deferred = Binding;
                Deferred.Renderer = "DeferredRenderer";
                if (Binding.bDefinesShader)
                {
                    Forward.PixelShader = "Forward/ForwardFragment.slang";
                    Forward.LightShader = "Light/ForwardLight.slang";
                    if (Binding.Surface == EMaterialSurface::Translucent)
                    {
                        Deferred.PixelShader = Forward.PixelShader;
                        Deferred.LightShader = Forward.LightShader;
                    }
                    else
                    {
                        Deferred.PixelShader = "Deferred/GBufferFragment.slang";
                        Deferred.LightShader.clear();
                    }
                }

                Expanded.push_back(std::move(Forward));
                Expanded.push_back(std::move(Deferred));
            }

            Description.Bindings = std::move(Expanded);
            return true;
        }

        std::string MaterialShaderKey(const MaterialDescription& Description)
        {
            std::vector<const MaterialRendererBinding*> Ordered;
            Ordered.reserve(Description.Bindings.size());
            for (const MaterialRendererBinding& Binding : Description.Bindings)
            {
                Ordered.push_back(&Binding);
            }
            std::sort(
                Ordered.begin(),
                Ordered.end(),
                [](const MaterialRendererBinding* Left, const MaterialRendererBinding* Right)
                {
                    return Left->Renderer < Right->Renderer;
                });

            std::string Key;
            for (const MaterialRendererBinding* Binding : Ordered)
            {
                Key += Binding->Renderer;
                Key += ";";
                Key += Binding->VertexShader;
                Key += ";";
                Key += Binding->PixelShader;
                Key += ";";
                Key += Binding->MaterialShader;
                Key += ";";
                Key += Binding->LightShader;
                Key += ";";
                for (const MaterialScalarParameter& Scalar : Binding->ShaderScalars)
                {
                    Key += Scalar.Name;
                    Key += ":";
                    Key += std::to_string(Scalar.Components);
                    Key += ",";
                }
                Key += ";";
                for (const auto& Texture : Binding->ShaderTextures)
                {
                    Key += Texture.first;
                    Key += ",";
                }
                Key += "#";
            }
            return Key;
        }

        bool ValidateMergedMaterial(
            const MaterialDescription& Description,
            const std::string& FilePath,
            std::string* ErrorMessage)
        {
            if (Description.Bindings.empty())
            {
                SetErrorMessage(ErrorMessage, "Material description has no renderer binding: " + FilePath);
                return false;
            }

            for (const MaterialRendererBinding& Binding : Description.Bindings)
            {
                if (!Binding.bDefinesShader || Binding.VertexShader.empty() || Binding.PixelShader.empty() ||
                    Binding.MaterialShader.empty())
                {
                    SetErrorMessage(ErrorMessage, Binding.Renderer + " is missing shader paths: " + FilePath);
                    return false;
                }
                if ((Binding.Renderer == "ForwardRenderer" || Binding.Renderer == "DeferredRenderer") &&
                    (!Binding.bHasSurface || !Binding.bHasTwoSided))
                {
                    SetErrorMessage(
                        ErrorMessage,
                        Binding.Renderer + " is missing Surface or TwoSided: " + FilePath);
                    return false;
                }
            }
            return true;
        }
    }

    MaterialHandle AssetManager::FindOrCreateMaterial(
        const MaterialDescription& Description,
        std::string* ErrorMessage)
    {
        const std::string Key = MaterialShaderKey(Description);
        for (uint32 Index = 0; Index < SharedMaterialKeys.size(); ++Index)
        {
            if (SharedMaterialKeys[Index] == Key)
            {
                MaterialHandle Handle;
                Handle.Index = Index;
                return Handle;
            }
        }

        if (Shaders == nullptr)
        {
            SetErrorMessage(ErrorMessage, "Shader manager is not set");
            return {};
        }

        std::vector<MaterialShaderBinding> ShaderBindings;
        ShaderBindings.reserve(Description.Bindings.size());
        for (const MaterialRendererBinding& Binding : Description.Bindings)
        {
            if (!Shaders->HasSource(Binding.VertexShader) || !Shaders->HasSource(Binding.PixelShader) ||
                !Shaders->HasSource(Binding.MaterialShader) ||
                (!Binding.LightShader.empty() && !Shaders->HasSource(Binding.LightShader)))
            {
                SetErrorMessage(ErrorMessage, "Material shader is missing: " + Binding.MaterialShader);
                return {};
            }

            std::vector<std::string> DescriptorFields;
            DescriptorFields.reserve(Binding.ShaderTextures.size());
            for (const auto& Texture : Binding.ShaderTextures)
            {
                DescriptorFields.push_back(Texture.first);
            }

            const ShaderHandle Shader = Shaders->Register(
                Binding.VertexShader,
                Binding.PixelShader,
                Binding.MaterialShader,
                Binding.LightShader,
                DescriptorFields,
                ScalarFieldsFor(Binding));
            if (!Shader.IsValid() || !Shaders->Compile(Shader, ErrorMessage))
            {
                return {};
            }

            const ShaderReflection* Reflection = Shaders->GetReflection(Shader, ERHIShaderStage::Pixel);
            if (Reflection == nullptr)
            {
                SetErrorMessage(ErrorMessage, "Shader reflection is missing: " + Binding.MaterialShader);
                return {};
            }

            for (const ShaderBinding& Reflected : Reflection->Bindings)
            {
                if (Reflected.Kind == EShaderBindingKind::ConstantBuffer && Reflected.Size > 1024)
                {
                    SetErrorMessage(ErrorMessage, "Material constant buffer exceeds 1024 bytes: " + Binding.MaterialShader);
                    return {};
                }
            }

            MaterialShaderBinding ShaderBinding;
            ShaderBinding.Renderer = Binding.Renderer;
            ShaderBinding.Shader = Shader;
            MaterialParameter::BuildLayout(*Reflection, ShaderBinding.Layout);
            ShaderBindings.push_back(std::move(ShaderBinding));
        }

        MaterialHandle Handle;
        Handle.Index = static_cast<uint32>(Materials.size());
        auto Owned = std::make_unique<Material>(std::move(ShaderBindings));
        Materials.push_back(Owned.get());
        auto Asset = std::make_unique<MaterialAsset>(Key, std::move(Owned), Handle);
        if (!Registry.Register(std::move(Asset)))
        {
            Materials.pop_back();
            SetErrorMessage(ErrorMessage, "Unable to register material: " + Key);
            return {};
        }

        SharedMaterialKeys.push_back(Key);
        return Handle;
    }

    MaterialInstanceHandle AssetManager::LoadMaterialInstance(
        const std::filesystem::path& DescriptionPath,
        std::string* ErrorMessage)
    {
        if (DescriptionPath.empty())
        {
            SetErrorMessage(ErrorMessage, "Material description path is empty");
            return {};
        }

        const std::string PathKey = DescriptionPath.generic_string();
        for (uint32 Index = 0; Index < MaterialInstancePaths.size(); ++Index)
        {
            if (MaterialInstancePaths[Index] == PathKey)
            {
                MaterialInstanceHandle Handle;
                Handle.Index = Index;
                return Handle;
            }
        }

        std::vector<std::string> Stack;
        MaterialDescription Description;
        if (!ResolveMaterialDescription(DescriptionPath, Description, Stack, ErrorMessage) ||
            !ExpandPrincipledSurface(Description, PathKey, ErrorMessage) ||
            !ValidateMergedMaterial(Description, PathKey, ErrorMessage))
        {
            return {};
        }

        const MaterialHandle BaseMaterial = FindOrCreateMaterial(Description, ErrorMessage);
        if (!BaseMaterial.IsValid() || BaseMaterial.Index >= Materials.size())
        {
            return {};
        }
        const Material* Base = Materials[BaseMaterial.Index];

        std::vector<MaterialRendererParameters> ParameterSets;
        ParameterSets.reserve(Description.Bindings.size());
        for (const MaterialRendererBinding& Binding : Description.Bindings)
        {
            const MaterialParameterLayout* Layout = Base->FindLayout(Binding.Renderer);
            if (Layout == nullptr)
            {
                SetErrorMessage(ErrorMessage, Binding.Renderer + " layout is missing: " + PathKey);
                return {};
            }

            MaterialRendererParameters ParameterSet;
            ParameterSet.Renderer = Binding.Renderer;
            ParameterSet.Surface = Binding.Surface;
            ParameterSet.bTwoSided = Binding.bTwoSided;
            if (!MaterialParameter::BuildBlock(Binding, ParameterSet.Parameters, ErrorMessage))
            {
                return {};
            }

            for (const MaterialScalarParameter& Scalar : Binding.Scalars)
            {
                if (!Layout->Has(Scalar.Name))
                {
                    SetErrorMessage(
                        ErrorMessage,
                        Scalar.Name + " is not a parameter of " + Binding.Renderer + ": " + PathKey);
                    return {};
                }
            }

            const auto FindTexturePath = [&](const std::string& Name) -> const std::string*
            {
                for (const auto& Texture : Binding.Textures)
                {
                    if (Texture.first == Name)
                    {
                        return &Texture.second;
                    }
                }
                return nullptr;
            };
            for (const auto& Texture : Binding.Textures)
            {
                if (!Layout->HasTexture(Texture.first))
                {
                    SetErrorMessage(
                        ErrorMessage,
                        Texture.first + " is not a texture parameter of " + Binding.Renderer + ": " + PathKey);
                    return {};
                }
            }

            bool bLoadedTexture = false;
            for (const MaterialParameterLayout::Entry& Entry : Layout->Entries)
            {
                if (Entry.Type != EMaterialParameterType::Texture2D)
                {
                    continue;
                }

                const std::string* Path = FindTexturePath(Entry.Name);
                if (Path == nullptr || Path->empty())
                {
                    SetErrorMessage(ErrorMessage, Entry.Name + " is missing from material parameters: " + PathKey);
                    return {};
                }

                const TextureAssetHandle Handle = LoadTexture(*Path, ErrorMessage);
                if (!Handle.IsValid())
                {
                    return {};
                }

                MaterialParameter::SetTexture(ParameterSet.Parameters, Entry.Name, Handle);
                bLoadedTexture = true;
            }

            if (Layout->Has("Sampler") || bLoadedTexture)
            {
                MaterialParameter::SetSampler(ParameterSet.Parameters, DefaultSampler());
            }

            if (!MaterialParameter::HasResources(ParameterSet.Parameters, *Layout))
            {
                SetErrorMessage(
                    ErrorMessage,
                    Binding.Renderer + " parameters do not match the shader parameter layout: " + PathKey);
                return {};
            }

            ParameterSets.push_back(std::move(ParameterSet));
        }

        MaterialInstances.push_back(std::make_unique<MaterialInstance>(BaseMaterial, std::move(ParameterSets)));
        MaterialInstancePaths.push_back(PathKey);
        MaterialInstanceHandle Handle;
        Handle.Index = static_cast<uint32>(MaterialInstances.size() - 1);
        return Handle;
    }

    SamplerHandle AssetManager::DefaultSampler()
    {
        if (!LinearSampler.IsValid())
        {
            LinearSampler.Index = 0;
        }

        return LinearSampler;
    }

    TextureAssetHandle AssetManager::LoadTexture(
        const std::filesystem::path& Path,
        std::string* ErrorMessage)
    {
        const std::string Name = Path.generic_string();
        constexpr std::string_view Suffix = ".texture.json";
        const bool bDescription = Name.size() >= Suffix.size() &&
            Name.compare(Name.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
        if (!bDescription)
        {
            return LoadTextureImage(Path, Path, ETextureEncoding::Linear, ErrorMessage);
        }

        TextureDescription Description;
        if (!DescriptionParser::LoadTextureDescription(Path, Description, ErrorMessage))
        {
            return {};
        }

        return LoadTextureImage(Path, Description.Source, Description.Encoding, ErrorMessage);
    }

    TextureAssetHandle AssetManager::LoadTextureImage(
        const std::filesystem::path& RegistryPath,
        const std::filesystem::path& ImagePath,
        ETextureEncoding Encoding,
        std::string* ErrorMessage)
    {
        if (RegistryPath.empty() || ImagePath.empty())
        {
            SetErrorMessage(ErrorMessage, "Texture path is empty");
            return {};
        }

        if (const TextureAsset* Existing = Registry.FindTexture(RegistryPath))
        {
            for (uint32 Index = 0; Index < Textures.size(); ++Index)
            {
                if (Textures[Index] == Existing)
                {
                    TextureAssetHandle Handle;
                    Handle.Index = Index;
                    return Handle;
                }
            }

            SetErrorMessage(ErrorMessage, "Texture is registered without a handle: " + RegistryPath.generic_string());
            return {};
        }

        const std::filesystem::path CookedPath = CookedTexturePath(ImagePath);
        CookedTexture Cooked;
        if (!LoadCookedTexture(CookedPath, Cooked, ErrorMessage))
        {
            return {};
        }

        if (Cooked.Format != ECookedTextureFormat::RGBA8_UNorm)
        {
            SetErrorMessage(ErrorMessage, "Texture is not an RGBA8 image: " + ImagePath.generic_string());
            return {};
        }

        std::vector<TextureAsset::Mip> Mips;
        Mips.reserve(Cooked.Mips.size());
        for (CookedTextureMip& Mip : Cooked.Mips)
        {
            TextureAsset::Mip Copy;
            Copy.Width = Mip.Width;
            Copy.Height = Mip.Height;
            Copy.Pixels = std::move(Mip.Pixels);
            Mips.push_back(std::move(Copy));
        }

        const uint32 Width = Mips.empty() ? 0 : Mips.front().Width;
        const uint32 Height = Mips.empty() ? 0 : Mips.front().Height;
        std::unique_ptr<TextureAsset> Texture = TextureAsset::Adopt(
            RegistryPath,
            std::move(Mips),
            ETextureFormat::RGBA8_UNorm,
            Encoding);
        if (Texture == nullptr)
        {
            return {};
        }

        const uint32 MipCount = static_cast<uint32>(Texture->GetMips().size());
        const TextureAsset* Loaded = Texture.get();
        if (!Registry.Register(std::move(Texture)))
        {
            SetErrorMessage(ErrorMessage, "Unable to register texture: " + RegistryPath.generic_string());
            return {};
        }

        Textures.push_back(Loaded);
        Log::Info(
            "Loaded texture {} from {} ({}x{}, {} mips, {})",
            RegistryPath.generic_string(),
            ImagePath.generic_string(),
            Width,
            Height,
            MipCount,
            Encoding == ETextureEncoding::sRGB ? "sRGB" : "Linear");
        TextureAssetHandle Handle;
        Handle.Index = static_cast<uint32>(Textures.size() - 1);
        return Handle;
    }

    TextureAssetHandle AssetManager::LoadEnvironmentMap(
        const std::filesystem::path& Path,
        std::string* ErrorMessage)
    {
        if (Path.empty())
        {
            SetErrorMessage(ErrorMessage, "Environment map path is empty");
            return {};
        }

        if (const TextureAsset* Existing = Registry.FindTexture(Path))
        {
            for (uint32 Index = 0; Index < Textures.size(); ++Index)
            {
                if (Textures[Index] == Existing)
                {
                    TextureAssetHandle Handle;
                    Handle.Index = Index;
                    EnvironmentMap = Handle;
                    return Handle;
                }
            }

            SetErrorMessage(ErrorMessage, "Environment map is registered without a handle: " + Path.generic_string());
            return {};
        }

        int Width = 0;
        int Height = 0;
        int Channels = 0;
        float* Pixels = STBAdapter::LoadImageFloat(Path.generic_string().c_str(), &Width, &Height, &Channels, 4);
        if (Pixels == nullptr || Width <= 0 || Height <= 0)
        {
            const char* Reason = STBAdapter::GetFailureReason();
            SetErrorMessage(
                ErrorMessage,
                "Unable to load environment map " + Path.generic_string() + ": " + (Reason != nullptr ? Reason : "unknown error"));
            STBAdapter::FreeImage(Pixels);
            return {};
        }

        std::vector<TextureAsset::Mip> Mips;
        TextureAsset::Mip Copy;
        Copy.Width = static_cast<uint32>(Width);
        Copy.Height = static_cast<uint32>(Height);
        const size_t ByteCount = static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4 * sizeof(float);
        Copy.Pixels.resize(ByteCount);
        std::memcpy(Copy.Pixels.data(), Pixels, ByteCount);
        STBAdapter::FreeImage(Pixels);
        const uint32 LoadedWidth = Copy.Width;
        const uint32 LoadedHeight = Copy.Height;
        Mips.push_back(std::move(Copy));
        std::unique_ptr<TextureAsset> Texture = TextureAsset::Adopt(
            Path,
            std::move(Mips),
            ETextureFormat::RGBA32_Float,
            ETextureEncoding::Linear);
        if (Texture == nullptr)
        {
            return {};
        }

        const TextureAsset* Loaded = Texture.get();
        if (!Registry.Register(std::move(Texture)))
        {
            SetErrorMessage(ErrorMessage, "Unable to register environment map: " + Path.generic_string());
            return {};
        }

        Textures.push_back(Loaded);
        Log::Info(
            "Loaded environment map {} ({}x{})",
            Path.generic_string(),
            LoadedWidth,
            LoadedHeight);
        TextureAssetHandle Handle;
        Handle.Index = static_cast<uint32>(Textures.size() - 1);
        EnvironmentMap = Handle;
        return Handle;
    }

    TextureAssetHandle AssetManager::GetEnvironmentMap() const
    {
        return EnvironmentMap;
    }

    const MeshAsset* AssetManager::ResolveMesh(MeshHandle Handle) const
    {
        if (!Handle.IsValid() || Handle.Index >= Meshes.size())
        {
            return nullptr;
        }

        return Meshes[Handle.Index];
    }

    const TextureAsset* AssetManager::ResolveTexture(TextureAssetHandle Handle) const
    {
        if (!Handle.IsValid() || Handle.Index >= Textures.size())
        {
            return nullptr;
        }

        return Textures[Handle.Index];
    }

    const Material* AssetManager::ResolveMaterial(MaterialHandle Handle) const
    {
        if (!Handle.IsValid() || Handle.Index >= Materials.size())
        {
            return nullptr;
        }

        return Materials[Handle.Index];
    }

    const MaterialInstance* AssetManager::ResolveMaterialInstance(MaterialInstanceHandle Handle) const
    {
        if (!Handle.IsValid() || Handle.Index >= MaterialInstances.size())
        {
            return nullptr;
        }

        return MaterialInstances[Handle.Index].get();
    }

    const AssetRegistry& AssetManager::GetRegistry() const
    {
        return Registry;
    }
}
