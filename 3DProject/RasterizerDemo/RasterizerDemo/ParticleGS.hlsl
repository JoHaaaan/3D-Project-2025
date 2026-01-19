<<<<<<< Updated upstream
<<<<<<< Updated upstream
<<<<<<< Updated upstream
=======
=======
>>>>>>> Stashed changes
=======
>>>>>>> Stashed changes
// ParticleGS.hlsl

cbuffer ParticleCameraBuffer : register(b0)
{
    float4x4 viewProjection;
    float3 cameraRight;
    float pad0;
    float3 cameraUp;
    float pad1;
};

struct GS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float lifetime : TEXCOORD1;
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
    // Inaktiv partikel: skapa inga vertices => ritas inte
    if (input[0].lifetime < 0.0f)
        return;

    float3 particlePos = input[0].position;

    float quadSize = 0.3f;
    float3 right = cameraRight * quadSize;
    float3 up = cameraUp * quadSize;

    float3 v1 = particlePos - right + up; // TL
    float3 v2 = particlePos + right + up; // TR
    float3 v3 = particlePos - right - up; // BL
    float3 v4 = particlePos + right - up; // BR

    GS_OUTPUT o;
    o.color = input[0].color;

    // Column-major: mul(v, M)
    o.clipPosition = mul(float4(v1, 1.0f), viewProjection);
    o.uv = float2(0, 0);
    output.Append(o);

    o.clipPosition = mul(float4(v2, 1.0f), viewProjection);
    o.uv = float2(1, 0);
    output.Append(o);

    o.clipPosition = mul(float4(v3, 1.0f), viewProjection);
    o.uv = float2(0, 1);
    output.Append(o);

    output.RestartStrip();

    o.clipPosition = mul(float4(v2, 1.0f), viewProjection);
    o.uv = float2(1, 0);
    output.Append(o);

    o.clipPosition = mul(float4(v4, 1.0f), viewProjection);
    o.uv = float2(1, 1);
    output.Append(o);

    o.clipPosition = mul(float4(v3, 1.0f), viewProjection);
    o.uv = float2(0, 1);
    output.Append(o);
}
<<<<<<< Updated upstream
<<<<<<< Updated upstream
>>>>>>> Stashed changes
=======
>>>>>>> Stashed changes
=======
>>>>>>> Stashed changes
