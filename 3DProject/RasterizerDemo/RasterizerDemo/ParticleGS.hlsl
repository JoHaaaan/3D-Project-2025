// PARTICLE GEOMETRY SHADER
// Expands each point primitive into a camera-facing billboard quad.

cbuffer ParticleCameraBuffer : register(b0)
{
    float4x4 viewProjection;
    float3 cameraPosition;
    float padding0;
};

struct GS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float lifetime : TEXCOORD0;
};

struct GS_OUTPUT
{
    float4 clipPosition : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

[maxvertexcount(6)]
void main(point GS_INPUT input[1], inout TriangleStream<GS_OUTPUT> output)
{
    if (input[0].lifetime < 0.0f)
        return;

    float3 particlePosition = input[0].position;

    float3 front = normalize(cameraPosition - particlePosition);
    float3 worldUp = float3(0.0f, 1.0f, 0.0f);

    float3 right = normalize(cross(front, worldUp));
    float3 up = normalize(cross(right, front));

    float quadSize = 0.3f;
    right *= quadSize;
    up *= quadSize;

    float3 topLeft = particlePosition - right + up;
    float3 topRight = particlePosition + right + up;
    float3 bottomLeft = particlePosition - right - up;
    float3 bottomRight = particlePosition + right - up;

    GS_OUTPUT o;
    o.color = input[0].color;

    o.clipPosition = mul(float4(topLeft, 1.0f), viewProjection);
    o.uv = float2(0.0f, 0.0f);
    output.Append(o);

    o.clipPosition = mul(float4(topRight, 1.0f), viewProjection);
    o.uv = float2(1.0f, 0.0f);
    output.Append(o);

    o.clipPosition = mul(float4(bottomLeft, 1.0f), viewProjection);
    o.uv = float2(0.0f, 1.0f);
    output.Append(o);

    output.RestartStrip();

    o.clipPosition = mul(float4(topRight, 1.0f), viewProjection);
    o.uv = float2(1.0f, 0.0f);
    output.Append(o);

    o.clipPosition = mul(float4(bottomRight, 1.0f), viewProjection);
    o.uv = float2(1.0f, 1.0f);
    output.Append(o);

    o.clipPosition = mul(float4(bottomLeft, 1.0f), viewProjection);
    o.uv = float2(0.0f, 1.0f);
    output.Append(o);
}