// Particle Pixel Shader
// Renders particles with soft circular falloff

struct PS_INPUT
{
    float4 clipPosition : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_Target
{
    // UV goes from (0..1), move to center and create circular mask
    float2 centeredUV = input.uv * 2.0f - 1.0f;
    float radiusSquared = dot(centeredUV, centeredUV);

    // Soft edge falloff for smoother appearance
    const float FEATHER = 0.15f;
    float alphaMask = 1.0f - smoothstep(1.0f - FEATHER, 1.0f, radiusSquared);

    // Combine with particle's alpha
    float4 outputColor = input.color;
    outputColor.a *= alphaMask;

    // Discard fully transparent pixels
    if (outputColor.a <= 0.001f)
        discard;

    return outputColor;
}