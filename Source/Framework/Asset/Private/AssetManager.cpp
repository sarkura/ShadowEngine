#include "Framework/Asset/Public/AssetManager.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/Material/Public/Material.h"
#include "Framework/Material/Public/MaterialInstance.h"

#include <cgltf.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/geometric.hpp>

#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

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
            std::string* ErrorMessage)
        {
            if (Primitive.type != cgltf_primitive_type_triangles)
            {
                SetErrorMessage(ErrorMessage, "Mesh primitive is not a triangle list: " + SourcePath);
                return false;
            }

            const cgltf_accessor* Positions = nullptr;
            const cgltf_accessor* Normals = nullptr;
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
            }

            if (Positions == nullptr || Positions->type != cgltf_type_vec3)
            {
                SetErrorMessage(ErrorMessage, "Mesh is missing positions: " + SourcePath);
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
            const MaterialInstance* Material = nullptr;
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

                    if (!AppendPrimitive(Primitive, World, SourcePath, Block->Vertices, Block->Indices, ErrorMessage))
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

        bool ReadJsonObject(
            const std::filesystem::path& FilePath,
            const char* Kind,
            rapidjson::Document& Document,
            std::string* ErrorMessage)
        {
            std::ifstream File(FilePath, std::ios::binary);
            if (!File)
            {
                SetErrorMessage(ErrorMessage, std::string("Unable to open ") + Kind + ": " + FilePath.generic_string());
                return false;
            }

            const std::string Json{
                std::istreambuf_iterator<char>(File),
                std::istreambuf_iterator<char>()};

            Document.Parse(Json.data(), Json.size());
            if (Document.HasParseError())
            {
                std::ostringstream Message;
                Message << "Invalid JSON in " << FilePath.generic_string()
                        << " at offset " << Document.GetErrorOffset() << ": "
                        << rapidjson::GetParseError_En(Document.GetParseError());
                SetErrorMessage(ErrorMessage, Message.str());
                return false;
            }

            if (!Document.IsObject())
            {
                SetErrorMessage(
                    ErrorMessage,
                    std::string(Kind) + " must be an object: " + FilePath.generic_string());
                return false;
            }

            return true;
        }

        bool RequireMembers(
            const rapidjson::Value& Object,
            const std::filesystem::path& FilePath,
            const char* Kind,
            const char* const* Names,
            size_t NameCount,
            std::string* ErrorMessage)
        {
            for (auto Member = Object.MemberBegin(); Member != Object.MemberEnd(); ++Member)
            {
                bool bKnown = false;
                for (size_t Index = 0; Index < NameCount; ++Index)
                {
                    if (Member->name == Names[Index])
                    {
                        bKnown = true;
                        break;
                    }
                }
                if (!bKnown)
                {
                    SetErrorMessage(
                        ErrorMessage,
                        std::string(Kind) + " has unsupported member '" + Member->name.GetString() +
                            "': " + FilePath.generic_string());
                    return false;
                }
            }

            for (size_t Index = 0; Index < NameCount; ++Index)
            {
                if (!Object.HasMember(Names[Index]))
                {
                    SetErrorMessage(
                        ErrorMessage,
                        std::string(Kind) + " is missing '" + Names[Index] + "': " + FilePath.generic_string());
                    return false;
                }
            }

            return true;
        }

        bool ReadMeshDescription(
            const std::filesystem::path& FilePath,
            std::filesystem::path& SourcePath,
            std::vector<MeshBlock>& Blocks,
            std::string* ErrorMessage)
        {
            rapidjson::Document Document;
            if (!ReadJsonObject(FilePath, "Mesh description", Document, ErrorMessage))
            {
                return false;
            }

            const char* const Members[] = {"type", "source", "materials"};
            if (!RequireMembers(Document, FilePath, "Mesh description", Members, 3, ErrorMessage))
            {
                return false;
            }

            const rapidjson::Value& Type = Document["type"];
            if (!Type.IsString() || std::string(Type.GetString()) != "Mesh")
            {
                SetErrorMessage(ErrorMessage, "Mesh description type must be Mesh: " + FilePath.generic_string());
                return false;
            }

            const rapidjson::Value& Source = Document["source"];
            if (!Source.IsString() || Source.GetStringLength() == 0)
            {
                SetErrorMessage(ErrorMessage, "Mesh description source must be a path: " + FilePath.generic_string());
                return false;
            }

            const rapidjson::Value& Materials = Document["materials"];
            if (!Materials.IsArray() || Materials.Empty())
            {
                SetErrorMessage(ErrorMessage, "Mesh description materials must be a non-empty array: " + FilePath.generic_string());
                return false;
            }

            const char* const SlotMembers[] = {"slot", "material"};
            Blocks.clear();
            Blocks.reserve(Materials.Size());
            for (rapidjson::SizeType Index = 0; Index < Materials.Size(); ++Index)
            {
                const rapidjson::Value& Entry = Materials[Index];
                if (!Entry.IsObject())
                {
                    SetErrorMessage(ErrorMessage, "Mesh material entry must be an object: " + FilePath.generic_string());
                    return false;
                }

                if (!RequireMembers(Entry, FilePath, "Mesh material entry", SlotMembers, 2, ErrorMessage))
                {
                    return false;
                }

                const rapidjson::Value& Slot = Entry["slot"];
                const rapidjson::Value& Material = Entry["material"];
                if (!Slot.IsUint())
                {
                    SetErrorMessage(ErrorMessage, "Mesh material slot must be an unsigned integer: " + FilePath.generic_string());
                    return false;
                }
                if (!Material.IsString() || Material.GetStringLength() == 0)
                {
                    SetErrorMessage(ErrorMessage, "Mesh material path is empty: " + FilePath.generic_string());
                    return false;
                }
                if (FindBlock(Blocks, Slot.GetUint()) != nullptr)
                {
                    SetErrorMessage(
                        ErrorMessage,
                        "Mesh material slot " + std::to_string(Slot.GetUint()) + " is duplicated: " + FilePath.generic_string());
                    return false;
                }

                MeshBlock Block;
                Block.Slot = Slot.GetUint();
                Block.MaterialPath = Material.GetString();
                Blocks.push_back(std::move(Block));
            }

            SourcePath = Source.GetString();
            return true;
        }
    }

    AssetManager::~AssetManager()
    {
        Finalize();
    }

    void AssetManager::Finalize()
    {
        Registry.Clear();
        MaterialInstances.clear();
        Materials.clear();
    }

    const MeshAsset* AssetManager::LoadMesh(
        const std::filesystem::path& Path,
        std::string* ErrorMessage)
    {
        if (Path.empty())
        {
            SetErrorMessage(ErrorMessage, "Mesh path is empty");
            return nullptr;
        }

        if (const MeshAsset* Existing = Registry.FindMesh(Path))
        {
            return Existing;
        }

        const std::string DescriptionPath = Path.generic_string();
        std::filesystem::path SourceFile;
        std::vector<MeshBlock> Blocks;
        if (!ReadMeshDescription(Path, SourceFile, Blocks, ErrorMessage))
        {
            return nullptr;
        }

        for (MeshBlock& Block : Blocks)
        {
            Block.Material = LoadMaterialInstance(Block.MaterialPath, ErrorMessage);
            if (Block.Material == nullptr)
            {
                return nullptr;
            }
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
            return nullptr;
        }

        Result = cgltf_load_buffers(&Options, File.Data, SourcePath.c_str());
        if (Result != cgltf_result_success)
        {
            SetErrorMessage(
                ErrorMessage,
                std::string("Unable to load mesh buffers ") + SourcePath + ": " + ToString(Result));
            return nullptr;
        }

        Result = cgltf_validate(File.Data);
        if (Result != cgltf_result_success)
        {
            SetErrorMessage(
                ErrorMessage,
                std::string("Mesh is invalid ") + SourcePath + ": " + ToString(Result));
            return nullptr;
        }

        const cgltf_scene* MeshScene = File.Data->scene;
        if (MeshScene == nullptr && File.Data->scenes_count > 0)
        {
            MeshScene = &File.Data->scenes[0];
        }
        if (MeshScene == nullptr)
        {
            SetErrorMessage(ErrorMessage, std::string("Mesh has no scene: ") + SourcePath);
            return nullptr;
        }

        for (cgltf_size NodeIndex = 0; NodeIndex < MeshScene->nodes_count; ++NodeIndex)
        {
            if (!AppendNode(MeshScene->nodes[NodeIndex], *File.Data, SourcePath, Blocks, ErrorMessage))
            {
                return nullptr;
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
                return nullptr;
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
            return nullptr;
        }

        Log::Info(
            "Loaded mesh {} from {} ({} sections, {} vertices, {} indices)",
            DescriptionPath,
            SourcePath,
            Loaded->GetSections().size(),
            VertexCount,
            IndexCount);
        return Loaded;
    }

    const MaterialInstance* AssetManager::LoadMaterialInstance(
        const std::filesystem::path& DescriptionPath,
        std::string* ErrorMessage)
    {
        if (DescriptionPath.empty())
        {
            SetErrorMessage(ErrorMessage, "Material description path is empty");
            return nullptr;
        }

        const std::string DescriptionKey = DescriptionPath.generic_string();
        for (const std::unique_ptr<MaterialInstance>& Existing : MaterialInstances)
        {
            if (Existing->GetDescriptionPath().generic_string() == DescriptionKey)
            {
                return Existing.get();
            }
        }

        MaterialDescription Description;
        if (!LoadMaterialDescription(DescriptionPath, Description, ErrorMessage))
        {
            return nullptr;
        }

        const std::string ShaderKey = Description.ShaderPath.generic_string();
        const Material* Parent = nullptr;
        for (const std::unique_ptr<Material>& Existing : Materials)
        {
            if (Existing->GetShaderPath().generic_string() == ShaderKey)
            {
                Parent = Existing.get();
                break;
            }
        }
        if (Parent == nullptr)
        {
            Materials.push_back(std::make_unique<Material>(Description.ShaderPath));
            Parent = Materials.back().get();
        }

        MaterialInstances.push_back(
            std::make_unique<MaterialInstance>(*Parent, DescriptionPath, std::move(Description.Parameter)));
        return MaterialInstances.back().get();
    }

    const AssetRegistry& AssetManager::GetRegistry() const
    {
        return Registry;
    }
}
