// ========================================
// PARTICLE PIXEL SHADER
// ========================================
// Part 3 of 3: Particle Rendering Pipeline (VS -> GS -> PS)
// Creates soft circular particles with radial alpha falloff

struct PS_INPUT
{
    float4 clipPosition : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_Target
{
    // Transform UV from [0,1] to [-1,1] centered coordinates
    float2 centeredUV = input.uv * 2.0f - 1.0f;
    float radiusSquared = dot(centeredUV, centeredUV);

    // Smooth radial falloff for soft particle edges (no hard circle border)
    const float FEATHER = 0.15f;
    float alphaMask = 1.0f - smoothstep(1.0f - FEATHER, 1.0f, radiusSquared);

    float4 outputColor = input.color;
    outputColor.a *= alphaMask;

    // Early discard for performance (skip fully transparent fragments)
    if (outputColor.a <= 0.001f)
        discard;

    return outputColor;
}