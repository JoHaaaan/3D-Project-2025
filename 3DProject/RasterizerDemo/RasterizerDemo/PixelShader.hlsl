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
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

Texture2D shaderTexture : register(t0);
SamplerState samplerState : register(s0);

struct PS_OUTPUT
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Extra : SV_Target2; // här lägger vi worldPos
};

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT o;

    // Albedo: textur * materialDiffuse
    float3 albedo = materialDiffuse;
    o.Albedo = float4(albedo, 1.0f);

    // Normal: [-1,1] -> [0,1]
    float3 n = normalize(input.normal);
    o.Normal = float4(n * 0.5f + 0.5f, 1.0f);

    // Extra: world position
    o.Extra = float4(input.worldPos, 1.0f);

    return o;
}
