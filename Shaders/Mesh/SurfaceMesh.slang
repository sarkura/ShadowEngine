#include "MeshletCommon.slang"
#include "../Common/SceneConstants.slang"

cbuffer SceneConstants : register(b0)
{
    float4x4 ViewProjection;
    float4x4 World;
    float4x4 NormalMatrix;
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void MeshMain(
    uint3 GroupId : SV_GroupID,
    uint GroupThread : SV_GroupThreadID,
    out vertices VertexOutput Vertices[64],
    out indices uint3 Triangles[124])
{
    const GpuMeshlet Meshlet = MeshletRecords[GroupId.x];
    const bool bVisible = MeshletVisible(ViewProjection, World, Meshlet);
    uint VertexCount = 0;
    uint TriangleCount = 0;
    if (bVisible)
    {
        VertexCount = Meshlet.VertexCount;
        TriangleCount = Meshlet.PrimitiveCount;
    }
    SetMeshOutputCounts(VertexCount, TriangleCount);

    if (GroupThread < VertexCount)
    {
        const MeshVertexData Input = LoadMeshVertex(MeshletVertexIds[Meshlet.VertexOffset + GroupThread]);
        VertexOutput Output;
        const float4 WorldPosition = mul(World, float4(Input.Position, 1.0));
        Output.Position = mul(ViewProjection, WorldPosition);
        Output.WorldPosition = WorldPosition.xyz;
        Output.WorldNormal = mul((float3x3)NormalMatrix, Input.Normal);
        Output.Color = Input.Color;
        Output.TexCoord = Input.TexCoord;
        Vertices[GroupThread] = Output;
    }

    if (GroupThread < TriangleCount)
    {
        const uint Base = Meshlet.PrimitiveOffset + GroupThread * 3;
        Triangles[GroupThread] = uint3(
            MeshletPrimitives[Base],
            MeshletPrimitives[Base + 1],
            MeshletPrimitives[Base + 2]);
    }
}
