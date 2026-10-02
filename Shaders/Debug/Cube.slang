struct VertexInput
{
    float3 Position : POSITION;
    float3 Color : COLOR0;
};

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 Color : COLOR0;
};

static const float3 CameraPosition = float3(3.0, 2.0, 3.0);
static const float3 CameraTarget = float3(0.0, 0.0, 0.0);
static const float CameraFovY = 0.78539816;
static const float CameraAspect = 16.0 / 9.0;
static const float CameraNear = 0.1;
static const float CameraFar = 100.0;

[shader("vertex")]
VertexOutput VertexMain(VertexInput Input)
{
    float3 Forward = normalize(CameraTarget - CameraPosition);
    float3 Right = normalize(cross(Forward, float3(0.0, 1.0, 0.0)));
    float3 Up = cross(Right, Forward);
    float3 Offset = Input.Position - CameraPosition;

    float ViewX = dot(Offset, Right);
    float ViewY = dot(Offset, Up);
    float ViewZ = -dot(Offset, Forward);
    float TanHalfFovY = tan(CameraFovY * 0.5);

    VertexOutput Output;
    Output.Position = float4(
        ViewX / (CameraAspect * TanHalfFovY),
        ViewY / TanHalfFovY,
        ViewZ * (CameraFar / (CameraNear - CameraFar)) - (CameraFar * CameraNear) / (CameraFar - CameraNear),
        -ViewZ);
    Output.Color = Input.Color;
    return Output;
}

[shader("fragment")]
float4 PixelMain(VertexOutput Input) : SV_Target
{
    return float4(Input.Color, 1.0);
}
