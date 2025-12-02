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

    // Diffus färg = textur * materialets diffuse-komponent
    float3 texColor = shaderTexture.Sample(samplerState, input.uv).rgb;
    float3 diffuseColor = texColor * materialDiffuse;

    // Beräkna "styrka" för ambient och specular från materialfärgerna.
    // Här tar vi medelvärdet av RGB som ett enkelt mått i intervallet 0..1.
    float ambientStrength = saturate(dot(materialAmbient, float3(0.333f, 0.333f, 0.333f)));
    float specularStrength = saturate(dot(materialSpecular, float3(0.333f, 0.333f, 0.333f)));

    // Packa specularPower (t.ex. 0..256) in i alpha [0,1]
    float specPacked = saturate(specularPower / 256.0f);

    // RT0: diffus färg + ambient-styrka i alpha
    o.Albedo = float4(diffuseColor, ambientStrength);

    // RT1: packad normal + specular-styrka i alpha
    float3 n = normalize(input.normal);
    o.Normal = float4(n * 0.5f + 0.5f, specularStrength);

    // RT2: world position + shininess-packning i alpha
    o.Extra = float4(input.worldPos, specPacked);

    return o;
}

