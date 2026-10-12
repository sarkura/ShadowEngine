[shader("vertex")]
float4 VertexMain(uint VertexId : SV_VertexID) : SV_Position
{
    const float2 Uv = float2((VertexId << 1) & 2, VertexId & 2);
    return float4(Uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
}
