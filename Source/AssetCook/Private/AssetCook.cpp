#include "AssetCook/Public/AssetCook.h"

#include "ThirdPartyAdapter/Mathematics/Public/GLMAdapter.h"
#include "ThirdPartyAdapter/Mesh/Public/CGLTFAdapter.h"
#include "ThirdPartyAdapter/Mesh/Public/MeshOptimizerAdapter.h"
#include "ThirdPartyAdapter/Texture/Public/STBAdapter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

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

        void TransformPoint(const float* Matrix, const float In[3], float Out[3])
        {
            Out[0] = Matrix[0] * In[0] + Matrix[4] * In[1] + Matrix[8] * In[2] + Matrix[12];
            Out[1] = Matrix[1] * In[0] + Matrix[5] * In[1] + Matrix[9] * In[2] + Matrix[13];
            Out[2] = Matrix[2] * In[0] + Matrix[6] * In[1] + Matrix[10] * In[2] + Matrix[14];
        }

        void TransformNormal(const float* Matrix, const float In[3], float Out[3])
        {
            const Mat4 World = GLMAdapter::MakeMat4(Matrix);
            const Mat3 NormalMatrix = GLMAdapter::Transpose(GLMAdapter::Inverse(Mat3(World)));
            const Vec3 Normal = GLMAdapter::Normalize(NormalMatrix * Vec3(In[0], In[1], In[2]));
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

                const Vec3 Position0(Vertices[Index0].Position[0], Vertices[Index0].Position[1], Vertices[Index0].Position[2]);
                const Vec3 Position1(Vertices[Index1].Position[0], Vertices[Index1].Position[1], Vertices[Index1].Position[2]);
                const Vec3 Position2(Vertices[Index2].Position[0], Vertices[Index2].Position[1], Vertices[Index2].Position[2]);
                const Vec3 Face = GLMAdapter::Cross(Position1 - Position0, Position2 - Position0);
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
                const Vec3 Normal(Vertex.Normal[0], Vertex.Normal[1], Vertex.Normal[2]);
                const float Length = GLMAdapter::Length(Normal);
                const Vec3 Unit = Length > 0.0001F ? Normal / Length : Vec3(0.0F, 1.0F, 0.0F);
                Vertex.Normal[0] = Unit.x;
                Vertex.Normal[1] = Unit.y;
                Vertex.Normal[2] = Unit.z;
            }
        }

        void OptimizeSection(CookedMeshSection& Section)
        {
            if (Section.Vertices.empty() || Section.Indices.empty())
            {
                return;
            }

            std::vector<uint32> Cached(Section.Indices.size());
            MeshOptimizerAdapter::OptimizeVertexCache(
                Cached.data(),
                Section.Indices.data(),
                Section.Indices.size(),
                Section.Vertices.size());

            std::vector<uint32> Overdraw(Section.Indices.size());
            MeshOptimizerAdapter::OptimizeOverdraw(
                Overdraw.data(),
                Cached.data(),
                Cached.size(),
                Section.Vertices.front().Position,
                Section.Vertices.size(),
                sizeof(MeshVertex));

            std::vector<MeshVertex> Fetched(Section.Vertices.size());
            const size_t VertexCount = MeshOptimizerAdapter::OptimizeVertexFetch(
                Fetched.data(),
                Overdraw.data(),
                Overdraw.size(),
                Section.Vertices.data(),
                Section.Vertices.size(),
                sizeof(MeshVertex));
            Fetched.resize(VertexCount);
            Section.Vertices.swap(Fetched);
            Section.Indices.swap(Overdraw);
        }

        CookedMeshSection* FindSection(std::vector<CookedMeshSection>& Sections, uint32 Slot)
        {
            for (CookedMeshSection& Section : Sections)
            {
                if (Section.Slot == Slot)
                {
                    return &Section;
                }
            }

            return nullptr;
        }

        bool AppendPrimitive(
            const CGLTFPrimitive& Primitive,
            const float* World,
            const std::string& SourcePath,
            CookedMeshSection& Section,
            bool bRequireTexCoord,
            std::string* ErrorMessage)
        {
            const size_t Count = Primitive.Positions.size() / 3;
            if (Count == 0 || Primitive.Positions.size() != Count * 3)
            {
                SetError(ErrorMessage, "Mesh is missing positions: " + SourcePath);
                return false;
            }

            const bool bHasTexCoords = Primitive.TexCoords.size() == Count * 2;
            if (bRequireTexCoord && !bHasTexCoords)
            {
                SetError(ErrorMessage, "Mesh is missing texture coordinates: " + SourcePath);
                return false;
            }

            const bool bHasNormals = Primitive.Normals.size() == Count * 3;
            const uint32 BaseVertex = static_cast<uint32>(Section.Vertices.size());
            Section.Vertices.reserve(Section.Vertices.size() + Count);
            for (size_t VertexIndex = 0; VertexIndex < Count; ++VertexIndex)
            {
                MeshVertex Vertex{};
                TransformPoint(World, Primitive.Positions.data() + VertexIndex * 3, Vertex.Position);
                if (bHasNormals)
                {
                    TransformNormal(World, Primitive.Normals.data() + VertexIndex * 3, Vertex.Normal);
                }
                Vertex.Color[0] = Vertex.Position[0] * 0.5F + 0.5F;
                Vertex.Color[1] = Vertex.Position[1] * 0.5F + 0.5F;
                Vertex.Color[2] = Vertex.Position[2] * 0.5F + 0.5F;
                if (bHasTexCoords)
                {
                    Vertex.TexCoord[0] = Primitive.TexCoords[VertexIndex * 2];
                    Vertex.TexCoord[1] = Primitive.TexCoords[VertexIndex * 2 + 1];
                }
                Section.Vertices.push_back(Vertex);
            }

            if (bHasTexCoords)
            {
                Section.bHasTexCoords = true;
            }

            Section.Indices.reserve(Section.Indices.size() + Primitive.Indices.size());
            for (const uint32 Index : Primitive.Indices)
            {
                Section.Indices.push_back(BaseVertex + Index);
            }

            if (!bHasNormals)
            {
                BuildMissingNormals(Section.Vertices, Section.Indices, BaseVertex);
            }

            return true;
        }

        uint8 LinearToSrgbChannel(float Linear)
        {
            const float Clamped = std::clamp(Linear, 0.0F, 1.0F);
            const float Encoded = Clamped <= 0.0031308F
                ? Clamped * 12.92F
                : 1.055F * std::pow(Clamped, 1.0F / 2.4F) - 0.055F;
            return static_cast<uint8>(std::clamp(Encoded, 0.0F, 1.0F) * 255.0F + 0.5F);
        }

        float SrgbToLinearChannel(uint8 Encoded)
        {
            const float Value = static_cast<float>(Encoded) / 255.0F;
            if (Value <= 0.04045F)
            {
                return Value / 12.92F;
            }

            return std::pow((Value + 0.055F) / 1.055F, 2.4F);
        }

        void Downsample(
            const std::vector<uint8>& Source,
            uint32 SourceWidth,
            uint32 SourceHeight,
            uint32 DestWidth,
            uint32 DestHeight,
            std::vector<uint8>& Dest)
        {
            Dest.resize(static_cast<size_t>(DestWidth) * DestHeight * 4);
            for (uint32 Y = 0; Y < DestHeight; ++Y)
            {
                for (uint32 X = 0; X < DestWidth; ++X)
                {
                    const uint32 X0 = X * 2;
                    const uint32 Y0 = Y * 2;
                    const uint32 X1 = std::min(X0 + 1, SourceWidth - 1);
                    const uint32 Y1 = std::min(Y0 + 1, SourceHeight - 1);
                    const uint32 Offsets[4] = {
                        (Y0 * SourceWidth + X0) * 4,
                        (Y0 * SourceWidth + X1) * 4,
                        (Y1 * SourceWidth + X0) * 4,
                        (Y1 * SourceWidth + X1) * 4,
                    };
                    uint8* Pixel = Dest.data() + (static_cast<size_t>(Y) * DestWidth + X) * 4;
                    for (uint32 Channel = 0; Channel < 4; ++Channel)
                    {
                        const uint32 Sum = static_cast<uint32>(Source[Offsets[0] + Channel]) +
                            Source[Offsets[1] + Channel] +
                            Source[Offsets[2] + Channel] +
                            Source[Offsets[3] + Channel];
                        Pixel[Channel] = static_cast<uint8>((Sum + 2) / 4);
                    }
                }
            }
        }

        void DownsampleSrgb(
            const std::vector<uint8>& Source,
            uint32 SourceWidth,
            uint32 SourceHeight,
            uint32 DestWidth,
            uint32 DestHeight,
            std::vector<uint8>& Dest)
        {
            Dest.resize(static_cast<size_t>(DestWidth) * DestHeight * 4);
            for (uint32 Y = 0; Y < DestHeight; ++Y)
            {
                for (uint32 X = 0; X < DestWidth; ++X)
                {
                    const uint32 X0 = X * 2;
                    const uint32 Y0 = Y * 2;
                    const uint32 X1 = std::min(X0 + 1, SourceWidth - 1);
                    const uint32 Y1 = std::min(Y0 + 1, SourceHeight - 1);
                    const uint32 Offsets[4] = {
                        (Y0 * SourceWidth + X0) * 4,
                        (Y0 * SourceWidth + X1) * 4,
                        (Y1 * SourceWidth + X0) * 4,
                        (Y1 * SourceWidth + X1) * 4,
                    };
                    float Linear[3] = {};
                    uint32 Alpha = 0;
                    for (const uint32 Offset : Offsets)
                    {
                        Linear[0] += SrgbToLinearChannel(Source[Offset]);
                        Linear[1] += SrgbToLinearChannel(Source[Offset + 1]);
                        Linear[2] += SrgbToLinearChannel(Source[Offset + 2]);
                        Alpha += Source[Offset + 3];
                    }

                    uint8* Pixel = Dest.data() + (static_cast<size_t>(Y) * DestWidth + X) * 4;
                    Pixel[0] = LinearToSrgbChannel(Linear[0] * 0.25F);
                    Pixel[1] = LinearToSrgbChannel(Linear[1] * 0.25F);
                    Pixel[2] = LinearToSrgbChannel(Linear[2] * 0.25F);
                    Pixel[3] = static_cast<uint8>((Alpha + 2) / 4);
                }
            }
        }

        void BuildMips(CookedTexture& Texture, ECookedTextureEncoding Encoding)
        {
            uint32 Width = Texture.Mips.front().Width;
            uint32 Height = Texture.Mips.front().Height;
            while (Width > 1 || Height > 1)
            {
                const uint32 NextWidth = std::max<uint32>(1, Width / 2);
                const uint32 NextHeight = std::max<uint32>(1, Height / 2);
                CookedTextureMip Next;
                Next.Width = NextWidth;
                Next.Height = NextHeight;
                if (Encoding == ECookedTextureEncoding::sRGB)
                {
                    DownsampleSrgb(Texture.Mips.back().Pixels, Width, Height, NextWidth, NextHeight, Next.Pixels);
                }
                else
                {
                    Downsample(Texture.Mips.back().Pixels, Width, Height, NextWidth, NextHeight, Next.Pixels);
                }
                Texture.Mips.push_back(std::move(Next));
                Width = NextWidth;
                Height = NextHeight;
            }
        }
    }

    bool CookMesh(
        const std::filesystem::path& Source,
        const std::vector<CookMeshSlot>& Slots,
        const std::filesystem::path& Output,
        std::string* ErrorMessage)
    {
        const std::string SourcePath = Source.generic_string();
        std::vector<CGLTFMeshNode> Nodes;
        if (!CGLTFAdapter::LoadMeshNodes(SourcePath.c_str(), Nodes, ErrorMessage))
        {
            return false;
        }

        CookedMesh Mesh;
        Mesh.Sections.reserve(Slots.size());
        for (const CookMeshSlot& Slot : Slots)
        {
            CookedMeshSection Section;
            Section.Slot = Slot.Slot;
            Mesh.Sections.push_back(std::move(Section));
        }

        for (const CGLTFMeshNode& Node : Nodes)
        {
            for (const CGLTFPrimitive& Primitive : Node.Primitives)
            {
                CookedMeshSection* Section = FindSection(Mesh.Sections, Primitive.MaterialSlot);
                if (Section == nullptr)
                {
                    SetError(
                        ErrorMessage,
                        "Mesh primitive slot " + std::to_string(Primitive.MaterialSlot) + " has no material: " + SourcePath);
                    return false;
                }

                const CookMeshSlot* Slot = nullptr;
                for (const CookMeshSlot& Candidate : Slots)
                {
                    if (Candidate.Slot == Primitive.MaterialSlot)
                    {
                        Slot = &Candidate;
                        break;
                    }
                }

                if (!AppendPrimitive(Primitive, Node.World, SourcePath, *Section, Slot->bRequireTexCoord, ErrorMessage))
                {
                    return false;
                }
            }
        }

        for (CookedMeshSection& Section : Mesh.Sections)
        {
            if (Section.Vertices.empty() || Section.Indices.empty())
            {
                SetError(ErrorMessage, "Mesh material slot " + std::to_string(Section.Slot) + " has no triangles: " + SourcePath);
                return false;
            }

            OptimizeSection(Section);
        }

        return SaveCookedMesh(Output, Mesh, ErrorMessage);
    }

    bool CookTexture(
        const std::filesystem::path& Source,
        ECookedTextureEncoding Encoding,
        const std::filesystem::path& Output,
        std::string* ErrorMessage)
    {
        int Width = 0;
        int Height = 0;
        int Channels = 0;
        unsigned char* Pixels = STBAdapter::LoadImage(Source.generic_string().c_str(), &Width, &Height, &Channels, 4);
        if (Pixels == nullptr)
        {
            const char* Reason = STBAdapter::GetFailureReason();
            SetError(ErrorMessage, "Unable to load texture " + Source.generic_string() + ": " + (Reason != nullptr ? Reason : "unknown error"));
            return false;
        }

        CookedTexture Texture;
        Texture.Format = ECookedTextureFormat::RGBA8_UNorm;
        Texture.Encoding = Encoding;
        CookedTextureMip Mip;
        Mip.Width = static_cast<uint32>(Width);
        Mip.Height = static_cast<uint32>(Height);
        Mip.Pixels.assign(Pixels, Pixels + static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4);
        STBAdapter::FreeImage(Pixels);
        Texture.Mips.push_back(std::move(Mip));
        BuildMips(Texture, Encoding);
        return SaveCookedTexture(Output, Texture, ErrorMessage);
    }
}
