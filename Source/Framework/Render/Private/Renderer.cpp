#include "Framework/Render/Public/Renderer.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/RHI/Public/RHIDevice.h"
#include "Framework/Scene/Public/Entity.h"
#include "Framework/Shader/Public/ShaderManager.h"

#include <cgltf.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace ShadowEngine
{
    namespace
    {
        constexpr char CubeShaderPath[] = "Debug/Cube.slang";
        constexpr char CubeMeshPath[] = "Assets/Mesh/Cube.glb";
        constexpr char VertexEntryPoint[] = "VertexMain";
        constexpr char PixelEntryPoint[] = "PixelMain";
        constexpr uint32 ConstantAlignment = 256;

        struct GpuObjectConstants
        {
            float ViewProjection[16];
            float World[16];
        };

        static_assert(sizeof(GpuObjectConstants) <= ConstantAlignment);

        struct MeshVertex
        {
            float Position[3];
            float Color[3];
        };

        struct MeshData
        {
            std::vector<MeshVertex> Vertices;
            std::vector<uint32> Indices;
        };

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

        bool AppendPrimitive(
            const cgltf_primitive& Primitive,
            const cgltf_float* World,
            MeshData& Mesh,
            std::string* ErrorMessage)
        {
            if (Primitive.type != cgltf_primitive_type_triangles)
            {
                SetErrorMessage(ErrorMessage, "Mesh primitive is not a triangle list: " + std::string(CubeMeshPath));
                return false;
            }

            const cgltf_accessor* Positions = nullptr;
            for (cgltf_size AttributeIndex = 0; AttributeIndex < Primitive.attributes_count; ++AttributeIndex)
            {
                const cgltf_attribute& Attribute = Primitive.attributes[AttributeIndex];
                if (Attribute.type == cgltf_attribute_type_position)
                {
                    Positions = Attribute.data;
                }
            }

            if (Positions == nullptr || Positions->type != cgltf_type_vec3)
            {
                SetErrorMessage(ErrorMessage, "Mesh is missing positions: " + std::string(CubeMeshPath));
                return false;
            }

            const uint32 BaseVertex = static_cast<uint32>(Mesh.Vertices.size());
            Mesh.Vertices.reserve(Mesh.Vertices.size() + Positions->count);
            for (cgltf_size VertexIndex = 0; VertexIndex < Positions->count; ++VertexIndex)
            {
                float Position[3] = {};
                if (cgltf_accessor_read_float(Positions, VertexIndex, Position, 3) == 0)
                {
                    SetErrorMessage(ErrorMessage, "Unable to read mesh positions: " + std::string(CubeMeshPath));
                    return false;
                }

                MeshVertex Vertex{};
                TransformPoint(World, Position, Vertex.Position);
                Vertex.Color[0] = Vertex.Position[0] * 0.5F + 0.5F;
                Vertex.Color[1] = Vertex.Position[1] * 0.5F + 0.5F;
                Vertex.Color[2] = Vertex.Position[2] * 0.5F + 0.5F;
                Mesh.Vertices.push_back(Vertex);
            }

            if (Primitive.indices == nullptr)
            {
                Mesh.Indices.reserve(Mesh.Indices.size() + Positions->count);
                for (cgltf_size Index = 0; Index < Positions->count; ++Index)
                {
                    Mesh.Indices.push_back(BaseVertex + static_cast<uint32>(Index));
                }
                return true;
            }

            Mesh.Indices.reserve(Mesh.Indices.size() + Primitive.indices->count);
            for (cgltf_size Index = 0; Index < Primitive.indices->count; ++Index)
            {
                const cgltf_size VertexIndex = cgltf_accessor_read_index(Primitive.indices, Index);
                Mesh.Indices.push_back(BaseVertex + static_cast<uint32>(VertexIndex));
            }

            return true;
        }

        bool AppendNode(const cgltf_node* Node, MeshData& Mesh, std::string* ErrorMessage)
        {
            if (Node->mesh != nullptr)
            {
                cgltf_float World[16];
                cgltf_node_transform_world(Node, World);
                for (cgltf_size PrimitiveIndex = 0; PrimitiveIndex < Node->mesh->primitives_count; ++PrimitiveIndex)
                {
                    if (!AppendPrimitive(Node->mesh->primitives[PrimitiveIndex], World, Mesh, ErrorMessage))
                    {
                        return false;
                    }
                }
            }

            for (cgltf_size ChildIndex = 0; ChildIndex < Node->children_count; ++ChildIndex)
            {
                if (!AppendNode(Node->children[ChildIndex], Mesh, ErrorMessage))
                {
                    return false;
                }
            }

            return true;
        }

        bool LoadMesh(MeshData& Mesh, std::string* ErrorMessage)
        {
            cgltf_options Options{};
            GltfFile File;
            cgltf_result Result = cgltf_parse_file(&Options, CubeMeshPath, &File.Data);
            if (Result != cgltf_result_success)
            {
                SetErrorMessage(
                    ErrorMessage,
                    std::string("Unable to parse mesh ") + CubeMeshPath + ": " + ToString(Result));
                return false;
            }

            Result = cgltf_load_buffers(&Options, File.Data, CubeMeshPath);
            if (Result != cgltf_result_success)
            {
                SetErrorMessage(
                    ErrorMessage,
                    std::string("Unable to load mesh buffers ") + CubeMeshPath + ": " + ToString(Result));
                return false;
            }

            Result = cgltf_validate(File.Data);
            if (Result != cgltf_result_success)
            {
                SetErrorMessage(
                    ErrorMessage,
                    std::string("Mesh is invalid ") + CubeMeshPath + ": " + ToString(Result));
                return false;
            }

            const cgltf_scene* Scene = File.Data->scene;
            if (Scene == nullptr && File.Data->scenes_count > 0)
            {
                Scene = &File.Data->scenes[0];
            }
            if (Scene == nullptr)
            {
                SetErrorMessage(ErrorMessage, std::string("Mesh has no scene: ") + CubeMeshPath);
                return false;
            }

            for (cgltf_size NodeIndex = 0; NodeIndex < Scene->nodes_count; ++NodeIndex)
            {
                if (!AppendNode(Scene->nodes[NodeIndex], Mesh, ErrorMessage))
                {
                    return false;
                }
            }

            if (Mesh.Vertices.empty() || Mesh.Indices.empty())
            {
                SetErrorMessage(ErrorMessage, std::string("Mesh has no triangles: ") + CubeMeshPath);
                return false;
            }

            Log::Info(
                "Loaded mesh {} ({} vertices, {} indices)",
                CubeMeshPath,
                Mesh.Vertices.size(),
                Mesh.Indices.size());
            return true;
        }

        std::span<const uint8> AsBytes(const void* Data, size_t Size)
        {
            return {static_cast<const uint8*>(Data), Size};
        }
    }

    Renderer::~Renderer()
    {
        Finalize();
    }

    bool Renderer::Initialize(
        RHIDevice& InDevice,
        RHISwapChain& InSwapChain,
        ShaderManager& Shaders,
        const Scene& InScene,
        const RHIColor& InClearColor,
        std::string* ErrorMessage)
    {
        Device = &InDevice;
        SwapChain = &InSwapChain;
        ClearColor = InClearColor;

        VertexShader = Shaders.LoadShader(
            InDevice,
            CubeShaderPath,
            VertexEntryPoint,
            ERHIShaderStage::Vertex,
            ErrorMessage);
        if (VertexShader == nullptr)
        {
            return false;
        }

        PixelShader = Shaders.LoadShader(
            InDevice,
            CubeShaderPath,
            PixelEntryPoint,
            ERHIShaderStage::Pixel,
            ErrorMessage);
        if (PixelShader == nullptr)
        {
            return false;
        }

        MeshData Mesh;
        if (!LoadMesh(Mesh, ErrorMessage))
        {
            return false;
        }

        RHIBufferDesc VertexDesc;
        VertexDesc.Stride = sizeof(MeshVertex);
        VertexDesc.Data = AsBytes(Mesh.Vertices.data(), Mesh.Vertices.size() * sizeof(MeshVertex));
        VertexBuffer = InDevice.CreateVertexBuffer(VertexDesc, ErrorMessage);
        if (VertexBuffer == nullptr)
        {
            return false;
        }

        const uint32 MaxIndex = *std::max_element(Mesh.Indices.begin(), Mesh.Indices.end());
        const bool bUint16 = MaxIndex <= 0xFFFF;
        std::vector<uint16> Indices16;
        RHIBufferDesc IndexDesc;
        if (bUint16)
        {
            Indices16.reserve(Mesh.Indices.size());
            for (const uint32 Index : Mesh.Indices)
            {
                Indices16.push_back(static_cast<uint16>(Index));
            }
            IndexDesc.Stride = sizeof(uint16);
            IndexDesc.Data = AsBytes(Indices16.data(), Indices16.size() * sizeof(uint16));
            IndexBuffer = InDevice.CreateIndexBuffer(IndexDesc, ERHIIndexFormat::Uint16, ErrorMessage);
        }
        else
        {
            IndexDesc.Stride = sizeof(uint32);
            IndexDesc.Data = AsBytes(Mesh.Indices.data(), Mesh.Indices.size() * sizeof(uint32));
            IndexBuffer = InDevice.CreateIndexBuffer(IndexDesc, ERHIIndexFormat::Uint32, ErrorMessage);
        }
        if (IndexBuffer == nullptr)
        {
            return false;
        }

        IndexCount = static_cast<uint32>(Mesh.Indices.size());

        const uint32 EntityCount = static_cast<uint32>(InScene.GetEntities().size());
        ConstantCapacity = EntityCount == 0 ? 1 : EntityCount;
        ConstantBuffer = InDevice.CreateConstantBuffer(ConstantCapacity * ConstantAlignment, ErrorMessage);
        if (ConstantBuffer == nullptr)
        {
            return false;
        }

        constexpr RHIInputElement InputLayout[] = {
            {"POSITION", 0, 0, ERHIVertexFormat::Float32x3},
            {"COLOR", 0, 12, ERHIVertexFormat::Float32x3},
        };

        RHIGraphicsPipelineDesc PipelineDesc;
        PipelineDesc.VertexShader = VertexShader.get();
        PipelineDesc.PixelShader = PixelShader.get();
        PipelineDesc.RenderTargetFormat = InSwapChain.GetFormat();
        PipelineDesc.Topology = ERHIPrimitiveTopology::TriangleList;
        PipelineDesc.InputLayout = InputLayout;
        PipelineDesc.bEnableDepth = true;
        Pipeline = InDevice.CreateGraphicsPipeline(PipelineDesc, ErrorMessage);
        if (Pipeline == nullptr)
        {
            return false;
        }

        if (!Resize(InSwapChain.GetWidth(), InSwapChain.GetHeight(), ErrorMessage))
        {
            return false;
        }

        CommandList = InDevice.CreateCommandList(ErrorMessage);
        return CommandList != nullptr;
    }

    bool Renderer::Resize(uint32 Width, uint32 Height, std::string* ErrorMessage)
    {
        if (Device == nullptr)
        {
            SetErrorMessage(ErrorMessage, "Renderer is not initialized");
            return false;
        }

        auto NewDepth = Device->CreateDepthTexture(Width, Height, ErrorMessage);
        if (NewDepth == nullptr)
        {
            return false;
        }

        DepthBuffer = std::move(NewDepth);
        return true;
    }

    void Renderer::Finalize()
    {
        CommandList.reset();
        Pipeline.reset();
        DepthBuffer.reset();
        ConstantBuffer.reset();
        IndexBuffer.reset();
        VertexBuffer.reset();
        PixelShader.reset();
        VertexShader.reset();
        IndexCount = 0;
        ConstantCapacity = 0;
        SwapChain = nullptr;
        Device = nullptr;
    }

    bool Renderer::RenderFrame(const Scene& InScene)
    {
        if (Device == nullptr || SwapChain == nullptr || CommandList == nullptr || VertexBuffer == nullptr ||
            IndexBuffer == nullptr || ConstantBuffer == nullptr || DepthBuffer == nullptr)
        {
            return false;
        }

        const float Width = static_cast<float>(SwapChain->GetWidth());
        const float Height = static_cast<float>(SwapChain->GetHeight());
        const float Aspect = Height > 0.0F ? Width / Height : 1.0F;
        const glm::mat4 View = glm::lookAtRH(
            glm::vec3(0.0F, 4.0F, 14.0F),
            glm::vec3(0.0F, 0.0F, 0.0F),
            glm::vec3(0.0F, 1.0F, 0.0F));
        const glm::mat4 Projection = glm::perspectiveRH_ZO(glm::radians(45.0F), Aspect, 0.1F, 100.0F);
        const glm::mat4 ViewProjection = Projection * View;

        const std::deque<Entity>& Entities = InScene.GetEntities();
        if (Entities.size() > ConstantCapacity)
        {
            Log::Error("Scene has more entities than the constant buffer can hold");
            return false;
        }

        std::vector<uint8> Constants(Entities.size() * ConstantAlignment);
        for (size_t Index = 0; Index < Entities.size(); ++Index)
        {
            GpuObjectConstants Slot{};
            std::memcpy(Slot.ViewProjection, glm::value_ptr(ViewProjection), sizeof(Slot.ViewProjection));
            Entities[Index].GetTransform().WriteWorldMatrix(Slot.World);
            std::memcpy(Constants.data() + Index * ConstantAlignment, &Slot, sizeof(Slot));
        }
        if (!Constants.empty() && !ConstantBuffer->Update(0, Constants))
        {
            Log::Error("Failed to write scene constants");
            return false;
        }

        CommandList->Begin();
        CommandList->BeginRenderPass(SwapChain->GetCurrentBackBuffer(), DepthBuffer.get(), ClearColor);
        CommandList->SetViewport({0.0F, 0.0F, Width, Height, 0.0F, 1.0F});
        CommandList->SetScissor({
            0,
            0,
            static_cast<int32>(SwapChain->GetWidth()),
            static_cast<int32>(SwapChain->GetHeight())});
        CommandList->SetPipeline(*Pipeline);
        CommandList->SetVertexBuffer(*VertexBuffer);
        CommandList->SetIndexBuffer(*IndexBuffer);
        for (uint32 Index = 0; Index < static_cast<uint32>(Entities.size()); ++Index)
        {
            CommandList->SetConstantBuffer(*ConstantBuffer, Index * ConstantAlignment);
            CommandList->DrawIndexed(IndexCount);
        }
        CommandList->EndRenderPass();
        CommandList->End();

        Device->SubmitCommandList(*CommandList);
        const bool bPresented = SwapChain->Present();
        Device->WaitIdle();
        return bPresented;
    }
}
