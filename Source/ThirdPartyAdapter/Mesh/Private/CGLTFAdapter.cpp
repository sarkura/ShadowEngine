#include "ThirdPartyAdapter/Mesh/Public/CGLTFAdapter.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#undef CGLTF_IMPLEMENTATION

namespace ShadowEngine
{
    namespace
    {
        void SetError(std::string* ErrorMessage, const std::string& Message)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = Message;
            }
        }

        const char* ResultToString(cgltf_result Result)
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

        bool ReadPrimitive(
            const cgltf_data& Data,
            const cgltf_primitive& Primitive,
            const std::string& SourcePath,
            CGLTFPrimitive& Out,
            std::string* ErrorMessage)
        {
            if (Primitive.type != cgltf_primitive_type_triangles)
            {
                SetError(ErrorMessage, "Mesh primitive is not a triangle list: " + SourcePath);
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
                SetError(ErrorMessage, "Mesh is missing positions: " + SourcePath);
                return false;
            }

            if (Primitive.material == nullptr)
            {
                Out.MaterialSlot = 0;
            }
            else if (Data.materials == nullptr || Primitive.material < Data.materials ||
                Primitive.material >= Data.materials + Data.materials_count)
            {
                SetError(ErrorMessage, "Mesh primitive material is out of range: " + SourcePath);
                return false;
            }
            else
            {
                Out.MaterialSlot = static_cast<uint32>(Primitive.material - Data.materials);
            }

            Out.Positions.resize(static_cast<size_t>(Positions->count) * 3);
            for (cgltf_size VertexIndex = 0; VertexIndex < Positions->count; ++VertexIndex)
            {
                if (cgltf_accessor_read_float(Positions, VertexIndex, Out.Positions.data() + VertexIndex * 3, 3) == 0)
                {
                    SetError(ErrorMessage, "Unable to read mesh positions: " + SourcePath);
                    return false;
                }
            }

            if (Normals != nullptr && Normals->type == cgltf_type_vec3)
            {
                Out.Normals.resize(static_cast<size_t>(Normals->count) * 3);
                for (cgltf_size VertexIndex = 0; VertexIndex < Normals->count; ++VertexIndex)
                {
                    if (cgltf_accessor_read_float(Normals, VertexIndex, Out.Normals.data() + VertexIndex * 3, 3) == 0)
                    {
                        SetError(ErrorMessage, "Unable to read mesh normals: " + SourcePath);
                        return false;
                    }
                }
            }

            if (TexCoords != nullptr && TexCoords->type == cgltf_type_vec2)
            {
                Out.TexCoords.resize(static_cast<size_t>(TexCoords->count) * 2);
                for (cgltf_size VertexIndex = 0; VertexIndex < TexCoords->count; ++VertexIndex)
                {
                    if (cgltf_accessor_read_float(TexCoords, VertexIndex, Out.TexCoords.data() + VertexIndex * 2, 2) == 0)
                    {
                        SetError(ErrorMessage, "Unable to read mesh texture coordinates: " + SourcePath);
                        return false;
                    }
                }
            }

            const cgltf_size IndexCount = Primitive.indices == nullptr ? Positions->count : Primitive.indices->count;
            Out.Indices.resize(static_cast<size_t>(IndexCount));
            if (Primitive.indices == nullptr)
            {
                for (cgltf_size Index = 0; Index < Positions->count; ++Index)
                {
                    Out.Indices[static_cast<size_t>(Index)] = static_cast<uint32>(Index);
                }
            }
            else
            {
                for (cgltf_size Index = 0; Index < Primitive.indices->count; ++Index)
                {
                    Out.Indices[static_cast<size_t>(Index)] =
                        static_cast<uint32>(cgltf_accessor_read_index(Primitive.indices, Index));
                }
            }

            return true;
        }

        bool AppendNode(
            const cgltf_node* Node,
            const cgltf_data& Data,
            const std::string& SourcePath,
            std::vector<CGLTFMeshNode>& Nodes,
            std::string* ErrorMessage)
        {
            if (Node->mesh != nullptr)
            {
                CGLTFMeshNode MeshNode;
                cgltf_node_transform_world(Node, MeshNode.World);
                MeshNode.Primitives.reserve(Node->mesh->primitives_count);
                for (cgltf_size PrimitiveIndex = 0; PrimitiveIndex < Node->mesh->primitives_count; ++PrimitiveIndex)
                {
                    CGLTFPrimitive Primitive;
                    if (!ReadPrimitive(Data, Node->mesh->primitives[PrimitiveIndex], SourcePath, Primitive, ErrorMessage))
                    {
                        return false;
                    }

                    MeshNode.Primitives.push_back(std::move(Primitive));
                }

                Nodes.push_back(std::move(MeshNode));
            }

            for (cgltf_size ChildIndex = 0; ChildIndex < Node->children_count; ++ChildIndex)
            {
                if (!AppendNode(Node->children[ChildIndex], Data, SourcePath, Nodes, ErrorMessage))
                {
                    return false;
                }
            }

            return true;
        }
    }

    bool CGLTFAdapter::LoadMeshNodes(
        const char* Path,
        std::vector<CGLTFMeshNode>& Nodes,
        std::string* ErrorMessage)
    {
        Nodes.clear();
        cgltf_options Options{};
        GltfFile File;
        cgltf_result Result = cgltf_parse_file(&Options, Path, &File.Data);
        if (Result != cgltf_result_success)
        {
            SetError(ErrorMessage, std::string("Unable to parse mesh ") + Path + ": " + ResultToString(Result));
            return false;
        }

        Result = cgltf_load_buffers(&Options, File.Data, Path);
        if (Result != cgltf_result_success)
        {
            SetError(ErrorMessage, std::string("Unable to load mesh buffers ") + Path + ": " + ResultToString(Result));
            return false;
        }

        Result = cgltf_validate(File.Data);
        if (Result != cgltf_result_success)
        {
            SetError(ErrorMessage, std::string("Mesh is invalid ") + Path + ": " + ResultToString(Result));
            return false;
        }

        const cgltf_scene* MeshScene = File.Data->scene;
        if (MeshScene == nullptr && File.Data->scenes_count > 0)
        {
            MeshScene = &File.Data->scenes[0];
        }
        if (MeshScene == nullptr)
        {
            SetError(ErrorMessage, std::string("Mesh has no scene: ") + Path);
            return false;
        }

        for (cgltf_size NodeIndex = 0; NodeIndex < MeshScene->nodes_count; ++NodeIndex)
        {
            if (!AppendNode(MeshScene->nodes[NodeIndex], *File.Data, Path, Nodes, ErrorMessage))
            {
                return false;
            }
        }

        return true;
    }
}
