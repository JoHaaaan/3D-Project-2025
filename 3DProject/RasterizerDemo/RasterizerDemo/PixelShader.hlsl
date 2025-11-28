cbuffer LightBuffer : register(b1)
{
    float3 lightPosition;
    float lightIntensity;
    float3 lightColor;
    float padding_Light;
};

cbuffer MaterialBuffer : register(b2)
{
    float3 materialAmbient;
    float padding1;

    float3 materialDiffuse;
    float padding2;

    float3 materialSpecular;
    float specularPower;
};

cbuffer CameraBuffer : register(b3)
{
    float3 cameraPosition;
    float padding_Camera;
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // clip-space position
    float3 worldPos : WORLD_POSITION; // world-space position
    float3 normal : NORMAL; // world-space normal
    float2 uv : TEXCOORD0; // texture coordinates
};

Texture2D shaderTexture : register(t0);
SamplerState samplerState : register(s0);

struct PS_OUTPUT
{
    float4 Albedo : SV_Target0; // goes to gAlbedo (t0 in compute)
    float4 Normal : SV_Target1; // goes to gNormal (t1 in compute)
    float4 Extra : SV_Target2; // goes to gWorldPos (t2 in compute)
};

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT o;

    // Sample the base color from the texture
    float4 texColor = shaderTexture.Sample(samplerState, input.uv);

    // Final albedo is texture color modulated by material diffuse color
    float3 albedo = texColor.rgb * materialDiffuse;
    o.Albedo = float4(albedo, 1.0f);

    // Store normal in [0, 1] instead of [-1, 1]
    float3 n = normalize(input.normal);
    float3 packedNormal = n * 0.5f + 0.5f;
    o.Normal = float4(packedNormal, 1.0f);

    // Store world position in RT2 so compute shader can use it
    o.Extra = float4(input.worldPos, 1.0f);

    return o;
}
