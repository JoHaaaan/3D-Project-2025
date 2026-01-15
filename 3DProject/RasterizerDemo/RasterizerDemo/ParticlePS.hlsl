// Input från geometry shader
struct PS_INPUT
{
    float4 clipPosition : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    // Enkel version: bara returnera färgen
    return input.color;
    
    // OPTIONAL: Lägg till textur om du vill
    // Texture2D particleTexture : register(t0);
    // SamplerState samplerState : register(s0);
    // float4 texColor = particleTexture.Sample(samplerState, input.uv);
    // return texColor * input.color;
}