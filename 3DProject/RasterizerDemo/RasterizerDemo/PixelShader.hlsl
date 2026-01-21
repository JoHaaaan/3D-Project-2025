// Pixel Shader
// Outputs G-Buffer data for deferred rendering

cbuffer MaterialBuffer : register(b2)
{
    float3 materialAmbient;
    float padding1;
    float3 materialDiffuse;
    float padding2;
    float3 materialSpecular;
    float specularPower;
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

Texture2D shaderTexture : register(t0);
SamplerState samplerState : register(s0);

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;

    // Sample diffuse texture
    float3 texColor = shaderTexture.Sample(samplerState, input.uv).rgb;
    float3 diffuseColor = texColor * materialDiffuse;

    // Calculate material strength values
    float ambientStrength = saturate(dot(materialAmbient, float3(0.333f, 0.333f, 0.333f)));
    float specularStrength = saturate(dot(materialSpecular, float3(0.333f, 0.333f, 0.333f)));
    float specularPacked = saturate(specularPower / 256.0f);

    // RT0: Albedo + ambient strength
    output.Albedo = float4(diffuseColor, ambientStrength);

    // RT1: Packed normal + specular strength
    float3 normalizedNormal = normalize(input.worldNormal);
    output.Normal = float4(normalizedNormal * 0.5f + 0.5f, specularStrength);

    // RT2: World position + shininess
    output.Extra = float4(input.worldPosition, specularPacked);

    return output;
}

