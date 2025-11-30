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
    float specularPower; // t.ex. upp till ~256
};

cbuffer CameraBuffer : register(b3)
{
    float3 cameraPosition;
    float padding_Camera;
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // clip-space
    float3 worldPos : WORLD_POSITION; // world-space position
    float3 normal : NORMAL; // world-space normal
    float2 uv : TEXCOORD0; // UV
};

Texture2D shaderTexture : register(t0);
SamplerState samplerState : register(s0);

struct PS_OUTPUT
{
    float4 Albedo : SV_Target0; // färg + specPower
    float4 Normal : SV_Target1; // packad normal
    float4 Extra : SV_Target2; // worldPos (eller annat)
};

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT o;

    // 1) Albedo = textur * materialDiffuse
    float3 texColor = shaderTexture.Sample(samplerState, input.uv).rgb;
    float3 albedo = texColor * materialDiffuse;

    // Packa specularPower (t.ex. 0..256) in i alpha [0,1]
    float specPacked = saturate(specularPower / 256.0f);

    o.Albedo = float4(albedo, specPacked);

    // 2) Normal: [-1,1] -> [0,1]
    float3 n = normalize(input.normal);
    o.Normal = float4(n * 0.5f + 0.5f, 1.0f);

    // 3) Extra: world position
    o.Extra = float4(input.worldPos, 1.0f);

    return o;
}
