// Reflection Pixel Shader
// Uses cube map for reflection mapping

cbuffer CameraBuffer : register(b3)
{
    float3 cameraPosition;
    float padding;
};

struct PS_INPUT
{
    float4 clipPosition : SV_POSITION;
    float3 worldPosition : WORLD_POSITION;
    float3 worldNormal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Extra : SV_Target2;
};

TextureCube reflectionTexture : register(t1);
SamplerState samplerState : register(s0);

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
    
    // Normalize the normal
    float3 normalizedNormal = normalize(input.worldNormal);
    
    // Calculate incoming view direction
    float3 incomingView = normalize(input.worldPosition - cameraPosition);
    
    // Calculate reflected view vector
    float3 reflectedView = reflect(incomingView, normalizedNormal);
    
    // Sample reflection texture
    float4 sampledValue = reflectionTexture.Sample(samplerState, reflectedView);
    
    // Output to G-Buffer format
    output.Albedo = float4(sampledValue.rgb, 0.2f);
    output.Normal = float4(normalizedNormal * 0.5f + 0.5f, 0.5f);
    output.Extra = float4(input.worldPosition, 0.5f);

    return output;
}
