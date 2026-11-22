#include "MeshletCommon.slang"

struct PositionOutput
{
    float4 Position : SV_Position;
};

cbuffer SceneConstants : register(b0)
{
    float4x4 ViewProjection;
    float4x4 World;
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void MeshMain(
    uint3 GroupId : SV_GroupID,
    uint GroupThread : SV_GroupThreadID,
    out vertices PositionOutput Vertices[64],
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
        PositionOutput Output;
        Output.Position = mul(ViewProjection, mul(World, float4(Input.Position, 1.0)));
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
