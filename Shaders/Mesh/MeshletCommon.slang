#pragma pack_matrix(column_major)

struct GpuMeshlet
{
    uint VertexOffset;
    uint VertexCount;
    uint PrimitiveOffset;
    uint PrimitiveCount;
    float CenterX;
    float CenterY;
    float CenterZ;
    float Radius;
};

struct MeshVertexData
{
    float3 Position;
    float3 Normal;
    float3 Color;
    float2 TexCoord;
};

ByteAddressBuffer MeshVertices : register(t0);
StructuredBuffer<uint> MeshletVertexIds : register(t1);
StructuredBuffer<uint> MeshletPrimitives : register(t2);
StructuredBuffer<GpuMeshlet> MeshletRecords : register(t3);

MeshVertexData LoadMeshVertex(uint Index)
{
    const uint Address = Index * 44;
    MeshVertexData Vertex;
    Vertex.Position = asfloat(MeshVertices.Load3(Address));
    Vertex.Normal = asfloat(MeshVertices.Load3(Address + 12));
    Vertex.Color = asfloat(MeshVertices.Load3(Address + 24));
    Vertex.TexCoord = asfloat(MeshVertices.Load2(Address + 36));
    return Vertex;
}

bool MeshletVisible(float4x4 ViewProjection, float4x4 World, GpuMeshlet Meshlet)
{
    const float3 Center = mul(World, float4(Meshlet.CenterX, Meshlet.CenterY, Meshlet.CenterZ, 1.0)).xyz;
    const float3 AxisX = float3(World[0].x, World[1].x, World[2].x);
    const float3 AxisY = float3(World[0].y, World[1].y, World[2].y);
    const float3 AxisZ = float3(World[0].z, World[1].z, World[2].z);
    const float Radius = Meshlet.Radius * max(length(AxisX), max(length(AxisY), length(AxisZ)));
    const float4 Planes[6] =
    {
        ViewProjection[3] + ViewProjection[0],
        ViewProjection[3] - ViewProjection[0],
        ViewProjection[3] + ViewProjection[1],
        ViewProjection[3] - ViewProjection[1],
        ViewProjection[2],
        ViewProjection[3] - ViewProjection[2]
    };

    [unroll]
    for (uint Plane = 0; Plane < 6; ++Plane)
    {
        const float Length = length(Planes[Plane].xyz);
        const float4 Normalized = Length > 0.0001 ? Planes[Plane] / Length : Planes[Plane];
        if (dot(Normalized.xyz, Center) + Normalized.w < -Radius)
        {
            return false;
        }
    }

    return true;
}
