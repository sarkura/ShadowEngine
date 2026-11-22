#include "MeshletCommon.slang"
#include "../Common/PerObject.slang"

struct VelocityOutput
{
    float4 Position : SV_Position;
    float4 CurrentClip : CURRENT_CLIP;
    float4 PreviousClip : PREVIOUS_CLIP;
};

cbuffer VelocityConstants : register(b0)
{
    PerObjectData PerObject;
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void MeshMain(
    uint3 GroupId : SV_GroupID,
    uint GroupThread : SV_GroupThreadID,
    out vertices VelocityOutput Vertices[64],
    out indices uint3 Triangles[124])
{
    const GpuMeshlet Meshlet = MeshletRecords[GroupId.x];
    const bool bVisible = MeshletVisible(PerObject.ViewProjection, PerObject.World, Meshlet);
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
        VelocityOutput Output;
        const float4 Position = float4(Input.Position, 1.0);
        Output.CurrentClip = mul(PerObject.ViewProjection, mul(PerObject.World, Position));
        Output.PreviousClip = mul(PerObject.PreviousViewProjection, mul(PerObject.PreviousWorld, Position));
        Output.Position = Output.CurrentClip;
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
