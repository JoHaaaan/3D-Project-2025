// Cube Map Pixel Shader
// Forward rendering with Blinn-Phong lighting for cube map generation

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
    float4 clipPosition : SV_POSITION;
    float3 worldPosition : WORLD_POSITION;
    float3 worldNormal : NORMAL;
    float2 uv : TEXCOORD0;
};

Texture2D shaderTexture : register(t0);
SamplerState samplerState : register(s0);

static const float3 LIGHT_DIRECTION = normalize(float3(-1.0f, -1.0f, 1.0f));
static const float3 LIGHT_COLOR = float3(1.0f, 0.95f, 0.9f);
static const float AMBIENT_STRENGTH = 0.3f;

float4 main(PS_INPUT input) : SV_Target
{
    float3 texColor = shaderTexture.Sample(samplerState, input.uv).rgb;
    float3 diffuseColor = texColor * materialDiffuse;
    
    float3 normalizedNormal = normalize(input.worldNormal);
    
    float3 ambient = AMBIENT_STRENGTH * diffuseColor;
    
    float NdotL = max(dot(normalizedNormal, -LIGHT_DIRECTION), 0.0f);
    float3 diffuse = NdotL * LIGHT_COLOR * diffuseColor;
  
    float3 viewDir = normalize(cameraPosition - input.worldPosition);
    float3 halfDir = normalize(-LIGHT_DIRECTION + viewDir);
    float NdotH = max(dot(normalizedNormal, halfDir), 0.0f);
    float specularFactor = pow(NdotH, max(specularPower, 1.0f));
    float3 specular = specularFactor * LIGHT_COLOR * materialSpecular * 0.5f;
  
    float3 finalColor = ambient + diffuse + specular;
    
    return float4(finalColor, 1.0f);
}
