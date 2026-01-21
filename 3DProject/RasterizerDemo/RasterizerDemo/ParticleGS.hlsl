
// Particle Geometry Shader
// Expands point primitives into camera-facing quads

cbuffer ParticleCameraBuffer : register(b0)
{
    float4x4 viewProjection;
    float3 cameraRight;
    float padding0;
    float3 cameraUp;
    float padding1;
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
    // Inactive particle: skip rendering
    if (input[0].lifetime < 0.0f)
        return;

    float3 particlePosition = input[0].position;

    float quadSize = 0.3f;
    float3 right = cameraRight * quadSize;
    float3 up = cameraUp * quadSize;

    // Calculate quad corners
    float3 vertex1 = particlePosition - right + up;
    float3 vertex2 = particlePosition + right + up;
    float3 vertex3 = particlePosition - right - up;
    float3 vertex4 = particlePosition + right - up;

    GS_OUTPUT o;
    o.color = input[0].color;

    // First triangle
    o.clipPosition = mul(float4(vertex1, 1.0f), viewProjection);
    o.uv = float2(0.0f, 0.0f);
    output.Append(o);

    o.clipPosition = mul(float4(vertex2, 1.0f), viewProjection);
    o.uv = float2(1.0f, 0.0f);
    output.Append(o);

    o.clipPosition = mul(float4(vertex3, 1.0f), viewProjection);
    o.uv = float2(0.0f, 1.0f);
    output.Append(o);

    output.RestartStrip();

    // Second triangle
    o.clipPosition = mul(float4(vertex2, 1.0f), viewProjection);
    o.uv = float2(1.0f, 0.0f);
    output.Append(o);

    o.clipPosition = mul(float4(vertex4, 1.0f), viewProjection);
    o.uv = float2(1.0f, 1.0f);
    output.Append(o);

    o.clipPosition = mul(float4(vertex3, 1.0f), viewProjection);
    o.uv = float2(0.0f, 1.0f);
    output.Append(o);
}

