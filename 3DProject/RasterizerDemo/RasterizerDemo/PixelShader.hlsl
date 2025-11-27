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

// --- G-buffer output ---
struct PS_OUTPUT
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Spec : SV_Target2;
};

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT o;

    // 1) Albedo: texturfärg * materialets diffuse
    float3 texColor = shaderTexture.Sample(samplerState, input.uv).rgb;
    float3 albedo = texColor * materialDiffuse;
    o.Albedo = float4(albedo, 1.0f);

    // 2) Normal: world normal packad från [-1,1] till [0,1]
    float3 n = normalize(input.normal);
    o.Normal = float4(n * 0.5f + 0.5f, 1.0f);

    // 3) Spec: specularfärg + shininess i alpha
    o.Spec = float4(materialSpecular, specularPower);

    return o;
}
