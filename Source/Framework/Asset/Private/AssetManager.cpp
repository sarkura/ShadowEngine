#include "Framework/Asset/Public/AssetManager.h"

#include "Framework/Asset/Public/MaterialAsset.h"
#include "Framework/Asset/Public/TextureAsset.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/Config/Public/DescriptionParser.h"
#include "Framework/Shader/Public/ShaderManager.h"
#include "Framework/Material/Public/Material.h"
#include "Framework/Material/Public/MaterialInstance.h"
#include "Framework/Material/Public/MaterialParameter.h"

#include <stb_image.h>

#include <cgltf.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/geometric.hpp>

#include <string>
#include <utility>
#include <vector>

namespace ShadowEngine
{
    namespace
    {
        struct GltfFile
        {
            cgltf_data* Data = nullptr;

            ~GltfFile()
            {
                if (Data != nullptr)
                {
                    cgltf_free(Data);
                }
            }
        };

        const char* ToString(cgltf_result Result)
        {
            switch (Result)
            {
                case cgltf_result_success:
                    return "success";
                case cgltf_result_data_too_short:
                    return "data too short";
                case cgltf_result_unknown_format:
                    return "unknown format";
                case cgltf_result_invalid_json:
                    return "invalid json";
                case cgltf_result_invalid_gltf:
                    return "invalid gltf";
                case cgltf_result_invalid_options:
                    return "invalid options";
                case cgltf_result_file_not_found:
                    return "file not found";
                case cgltf_result_io_error:
                    return "io error";
                case cgltf_result_out_of_memory:
                    return "out of memory";
                case cgltf_result_legacy_gltf:
                    return "legacy gltf";
                case cgltf_result_max_enum:
                    break;
            }

            return "unknown error";
        }

        void TransformPoint(const cgltf_float* Matrix, const float In[3], float Out[3])
        {
            Out[0] = Matrix[0] * In[0] + Matrix[4] * In[1] + Matrix[8] * In[2] + Matrix[12];
            Out[1] = Matrix[1] * In[0] + Matrix[5] * In[1] + Matrix[9] * In[2] + Matrix[13];
            Out[2] = Matrix[2] * In[0] + Matrix[6] * In[1] + Matrix[10] * In[2] + Matrix[14];
        }

        void TransformNormal(const cgltf_float* Matrix, const float In[3], float Out[3])
        {
            const glm::mat4 World = glm::make_mat4(Matrix);
            const glm::mat3 NormalMatrix = glm::transpose(glm::inverse(glm::mat3(World)));
            const glm::vec3 Normal = glm::normalize(NormalMatrix * glm::vec3(In[0], In[1], In[2]));
            Out[0] = Normal.x;
            Out[1] = Normal.y;
            Out[2] = Normal.z;
        }

        void BuildMissingNormals(std::vector<MeshVertex>& Vertices, const std::vector<uint32>& Indices, uint32 BaseVertex)
        {
            for (size_t Index = 0; Index + 2 < Indices.size(); Index += 3)
            {
                const uint32 Index0 = Indices[Index];
                const uint32 Index1 = Indices[Index + 1];
                const uint32 Index2 = Indices[Index + 2];
                if (Index0 < BaseVertex || Index1 < BaseVertex || Index2 < BaseVertex)
                {
                    continue;
                }

                const glm::vec3 Position0(Vertices[Index0].Position[0], Vertices[Index0].Position[1], Vertices[Index0].Position[2]);
                const glm::vec3 Position1(Vertices[Index1].Position[0], Vertices[Index1].Position[1], Vertices[Index1].Position[2]);
                const glm::vec3 Position2(Vertices[Index2].Position[0], Vertices[Index2].Position[1], Vertices[Index2].Position[2]);
                const glm::vec3 Face = glm::cross(Position1 - Position0, Position2 - Position0);
                Vertices[Index0].Normal[0] += Face.x;
                Vertices[Index0].Normal[1] += Face.y;
                Vertices[Index0].Normal[2] += Face.z;
                Vertices[Index1].Normal[0] += Face.x;
                Vertices[Index1].Normal[1] += Face.y;
                Vertices[Index1].Normal[2] += Face.z;
                Vertices[Index2].Normal[0] += Face.x;
                Vertices[Index2].Normal[1] += Face.y;
                Vertices[Index2].Normal[2] += Face.z;
            }

            for (size_t VertexIndex = BaseVertex; VertexIndex < Vertices.size(); ++VertexIndex)
            {
                MeshVertex& Vertex = Vertices[VertexIndex];
                const glm::vec3 Normal(Vertex.Normal[0], Vertex.Normal[1], Vertex.Normal[2]);
                const float Length = glm::length(Normal);
                const glm::vec3 Unit = Length > 0.0001F ? Normal / Length : glm::vec3(0.0F, 1.0F, 0.0F);
                Vertex.Normal[0] = Unit.x;
                Vertex.Normal[1] = Unit.y;
                Vertex.Normal[2] = Unit.z;
            }
        }

        bool AppendPrimitive(
            const cgltf_primitive& Primitive,
            const cgltf_float* World,
            const std::string& SourcePath,
            std::vector<MeshVertex>& Vertices,
            std::vector<uint32>& Indices,
            bool bRequireTexCoord,
            std::string* ErrorMessage)
        {
            if (Primitive.type != cgltf_primitive_type_triangles)
            {
                SetErrorMessage(ErrorMessage, "Mesh primitive is not a triangle list: " + SourcePath);
                return false;
            }

            const cgltf_accessor* Positions = nullptr;
            const cgltf_accessor* Normals = nullptr;
            const cgltf_accessor* TexCoords = nullptr;
            for (cgltf_size AttributeIndex = 0; AttributeIndex < Primitive.attributes_count; ++AttributeIndex)
            {
                const cgltf_attribute& Attribute = Primitive.attributes[AttributeIndex];
                if (Attribute.type == cgltf_attribute_type_position)
                {
                    Positions = Attribute.data;
                }
                else if (Attribute.type == cgltf_attribute_type_normal)
                {
                    Normals = Attribute.data;
                }
                else if (Attribute.type == cgltf_attribute_type_texcoord && Attribute.index == 0)
                {
                    TexCoords = Attribute.data;
                }
            }

            if (Positions == nullptr || Positions->type != cgltf_type_vec3)
            {
                SetErrorMessage(ErrorMessage, "Mesh is missing positions: " + SourcePath);
                return false;
            }

            if (bRequireTexCoord && (TexCoords == nullptr || TexCoords->type != cgltf_type_vec2))
            {
                SetErrorMessage(ErrorMessage, "Mesh is missing texture coordinates: " + SourcePath);
                return false;
            }

            const uint32 BaseVertex = static_cast<uint32>(Vertices.size());
            Vertices.reserve(Vertices.size() + Positions->count);
            for (cgltf_size VertexIndex = 0; VertexIndex < Positions->count; ++VertexIndex)
            {
                float Position[3] = {};
                if (cgltf_accessor_read_float(Positions, VertexIndex, Position, 3) == 0)
                {
                    SetErrorMessage(ErrorMessage, "Unable to read mesh positions: " + SourcePath);
                    return false;
                }

                MeshVertex Vertex{};
                TransformPoint(World, Position, Vertex.Position);
                if (Normals != nullptr && Normals->type == cgltf_type_vec3)
                {
                    float Normal[3] = {};
                    if (cgltf_accessor_read_float(Normals, VertexIndex, Normal, 3) == 0)
                    {
                        SetErrorMessage(ErrorMessage, "Unable to read mesh normals: " + SourcePath);
                        return false;
                    }
                    TransformNormal(World, Normal, Vertex.Normal);
                }
                Vertex.Color[0] = Vertex.Position[0] * 0.5F + 0.5F;
                Vertex.Color[1] = Vertex.Position[1] * 0.5F + 0.5F;
                Vertex.Color[2] = Vertex.Position[2] * 0.5F + 0.5F;
                if (TexCoords != nullptr && TexCoords->type == cgltf_type_vec2)
                {
                    float TexCoord[2] = {};
                    if (cgltf_accessor_read_float(TexCoords, VertexIndex, TexCoord, 2) == 0)
                    {
                        SetErrorMessage(ErrorMessage, "Unable to read mesh texture coordinates: " + SourcePath);
                        return false;
                    }
                    Vertex.TexCoord[0] = TexCoord[0];
                    Vertex.TexCoord[1] = TexCoord[1];
                }
                Vertices.push_back(Vertex);
            }

            if (Primitive.indices == nullptr)
            {
                Indices.reserve(Indices.size() + Positions->count);
                for (cgltf_size Index = 0; Index < Positions->count; ++Index)
                {
                    Indices.push_back(BaseVertex + static_cast<uint32>(Index));
                }
            }
            else
            {
                Indices.reserve(Indices.size() + Primitive.indices->count);
                for (cgltf_size Index = 0; Index < Primitive.indices->count; ++Index)
                {
                    const cgltf_size VertexIndex = cgltf_accessor_read_index(Primitive.indices, Index);
                    Indices.push_back(BaseVertex + static_cast<uint32>(VertexIndex));
                }
            }

            if (Normals == nullptr)
            {
                BuildMissingNormals(Vertices, Indices, BaseVertex);
            }

            return true;
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

        bool ReadPrimitiveSlot(
            const cgltf_data& Data,
            const cgltf_primitive& Primitive,
            const std::string& SourcePath,
            uint32& Slot,
            std::string* ErrorMessage)
        {
            if (Primitive.material == nullptr)
            {
                Slot = 0;
                return true;
            }

            if (Data.materials == nullptr || Primitive.material < Data.materials ||
                Primitive.material >= Data.materials + Data.materials_count)
            {
                SetErrorMessage(ErrorMessage, "Mesh primitive material is out of range: " + SourcePath);
                return false;
            }

            Slot = static_cast<uint32>(Primitive.material - Data.materials);
            return true;
        }

        bool AppendNode(
            const cgltf_node* Node,
            const cgltf_data& Data,
            const std::string& SourcePath,
            std::vector<MeshBlock>& Blocks,
            std::string* ErrorMessage)
        {
            if (Node->mesh != nullptr)
            {
                cgltf_float World[16];
                cgltf_node_transform_world(Node, World);
                for (cgltf_size PrimitiveIndex = 0; PrimitiveIndex < Node->mesh->primitives_count; ++PrimitiveIndex)
                {
                    const cgltf_primitive& Primitive = Node->mesh->primitives[PrimitiveIndex];
                    uint32 Slot = 0;
                    if (!ReadPrimitiveSlot(Data, Primitive, SourcePath, Slot, ErrorMessage))
                    {
                        return false;
                    }

                    MeshBlock* Block = FindBlock(Blocks, Slot);
                    if (Block == nullptr)
                    {
                        SetErrorMessage(
                            ErrorMessage,
                            "Mesh primitive slot " + std::to_string(Slot) + " has no material: " + SourcePath);
                        return false;
                    }

                    if (!AppendPrimitive(
                            Primitive,
                            World,
                            SourcePath,
                            Block->Vertices,
                            Block->Indices,
                            Block->bRequireTexCoord,
                            ErrorMessage))
                    {
                        return false;
                    }
                }
            }

            for (cgltf_size ChildIndex = 0; ChildIndex < Node->children_count; ++ChildIndex)
            {
                if (!AppendNode(Node->children[ChildIndex], Data, SourcePath, Blocks, ErrorMessage))
                {
                    return false;
                }
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
        Materials.clear();
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
            Block.Material = LoadMaterialInstance(Block.MaterialPath, ErrorMessage);
            if (!Block.Material.IsValid())
            {
                return {};
            }

            const MaterialInstance* Instance = ResolveMaterialInstance(Block.Material);
            Block.bRequireTexCoord =
                Instance != nullptr && MaterialParameter::GetTextureBinding(Instance->GetParameters()).HasTextures();
        }

        const std::string SourcePath = SourceFile.generic_string();
        cgltf_options Options{};
        GltfFile File;
        cgltf_result Result = cgltf_parse_file(&Options, SourcePath.c_str(), &File.Data);
        if (Result != cgltf_result_success)
        {
            SetErrorMessage(
                ErrorMessage,
                std::string("Unable to parse mesh ") + SourcePath + ": " + ToString(Result));
            return {};
        }

        Result = cgltf_load_buffers(&Options, File.Data, SourcePath.c_str());
        if (Result != cgltf_result_success)
        {
            SetErrorMessage(
                ErrorMessage,
                std::string("Unable to load mesh buffers ") + SourcePath + ": " + ToString(Result));
            return {};
        }

        Result = cgltf_validate(File.Data);
        if (Result != cgltf_result_success)
        {
            SetErrorMessage(
                ErrorMessage,
                std::string("Mesh is invalid ") + SourcePath + ": " + ToString(Result));
            return {};
        }

        const cgltf_scene* MeshScene = File.Data->scene;
        if (MeshScene == nullptr && File.Data->scenes_count > 0)
        {
            MeshScene = &File.Data->scenes[0];
        }
        if (MeshScene == nullptr)
        {
            SetErrorMessage(ErrorMessage, std::string("Mesh has no scene: ") + SourcePath);
            return {};
        }

        for (cgltf_size NodeIndex = 0; NodeIndex < MeshScene->nodes_count; ++NodeIndex)
        {
            if (!AppendNode(MeshScene->nodes[NodeIndex], *File.Data, SourcePath, Blocks, ErrorMessage))
            {
                return {};
            }
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

    MaterialInstanceHandle AssetManager::LoadMaterialInstance(
        const std::filesystem::path& DescriptionPath,
        std::string* ErrorMessage)
    {
        if (DescriptionPath.empty())
        {
            SetErrorMessage(ErrorMessage, "Material description path is empty");
            return {};
        }

        if (const MaterialAsset* Existing = Registry.FindMaterial(DescriptionPath))
        {
            for (uint32 Index = 0; Index < MaterialInstances.size(); ++Index)
            {
                if (MaterialInstances[Index]->GetBaseMaterial().Index == Existing->GetHandle().Index)
                {
                    MaterialInstanceHandle Handle;
                    Handle.Index = Index;
                    return Handle;
                }
            }
        }

        MaterialDescription Description;
        if (!DescriptionParser::LoadMaterialDescription(DescriptionPath, Description, ErrorMessage))
        {
            return {};
        }

        MaterialParameterLayout Layout;
        MaterialParameterBlock Parameters;
        if (!MaterialParameter::BuildLayout(Description, Layout, ErrorMessage) ||
            !MaterialParameter::BuildBlock(Description, Parameters, ErrorMessage))
        {
            return {};
        }

        if (!Description.BaseColorTexture.empty())
        {
            const TextureAssetHandle BaseColorTexture = LoadTexture(Description.BaseColorTexture, ErrorMessage);
            const TextureAssetHandle RoughnessTexture = LoadTexture(Description.RoughnessTexture, ErrorMessage);
            if (!BaseColorTexture.IsValid() || !RoughnessTexture.IsValid())
            {
                return {};
            }

            MaterialParameter::SetTextures(Parameters, BaseColorTexture, RoughnessTexture, DefaultSampler());
        }

        MaterialHandle BaseMaterial;
        if (const MaterialAsset* Existing = Registry.FindMaterial(DescriptionPath))
        {
            BaseMaterial = Existing->GetHandle();
        }
        else
        {
            if (Shaders == nullptr)
            {
                SetErrorMessage(ErrorMessage, "Shader manager is not set");
                return {};
            }

            const ShaderHandle Shader = Shaders->Register(Description.Shader);
            if (!Shader.IsValid())
            {
                SetErrorMessage(ErrorMessage, "Unable to register shader: " + Description.Shader);
                return {};
            }

            BaseMaterial.Index = static_cast<uint32>(Materials.size());
            auto Owned = std::make_unique<Material>(Shader, std::move(Layout));
            Materials.push_back(Owned.get());
            auto Asset = std::make_unique<MaterialAsset>(
                DescriptionPath,
                std::move(Owned),
                BaseMaterial);
            if (!Registry.Register(std::move(Asset)))
            {
                Materials.pop_back();
                SetErrorMessage(ErrorMessage, "Unable to register material: " + DescriptionPath.generic_string());
                return {};
            }
        }

        MaterialInstances.push_back(
            std::make_unique<MaterialInstance>(BaseMaterial, std::move(Parameters)));
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
        if (Path.empty())
        {
            SetErrorMessage(ErrorMessage, "Texture path is empty");
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
                    return Handle;
                }
            }

            SetErrorMessage(ErrorMessage, "Texture is registered without a handle: " + Path.generic_string());
            return {};
        }

        int Width = 0;
        int Height = 0;
        int Channels = 0;
        stbi_uc* Pixels = stbi_load(Path.generic_string().c_str(), &Width, &Height, &Channels, 4);
        if (Pixels == nullptr)
        {
            const char* Reason = stbi_failure_reason();
            SetErrorMessage(
                ErrorMessage,
                "Unable to load texture " + Path.generic_string() + ": " + (Reason != nullptr ? Reason : "unknown error"));
            return {};
        }

        const size_t ByteCount = static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4;
        std::vector<uint8> Rgba(Pixels, Pixels + ByteCount);
        stbi_image_free(Pixels);

        std::unique_ptr<TextureAsset> Texture = TextureAsset::Create(
            Path,
            static_cast<uint32>(Width),
            static_cast<uint32>(Height),
            std::move(Rgba),
            ErrorMessage);
        if (Texture == nullptr)
        {
            return {};
        }

        const uint32 MipCount = static_cast<uint32>(Texture->GetMips().size());
        const TextureAsset* Loaded = Texture.get();
        if (!Registry.Register(std::move(Texture)))
        {
            SetErrorMessage(ErrorMessage, "Unable to register texture: " + Path.generic_string());
            return {};
        }

        Textures.push_back(Loaded);
        Log::Info(
            "Loaded texture {} ({}x{}, {} mips)",
            Path.generic_string(),
            Width,
            Height,
            MipCount);
        TextureAssetHandle Handle;
        Handle.Index = static_cast<uint32>(Textures.size() - 1);
        return Handle;
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
