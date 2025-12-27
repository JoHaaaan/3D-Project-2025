// Simple forward rendering pixel shader for cube map rendering
// Outputs a single lit color instead of G-Buffer data

cbuffer MaterialBuffer : register(b2)
{
    float3 materialAmbient;
    float padding1;
    float3 materialDiffuse;
    float padding2;
    float3 materialSpecular;
    float specularPower;
};

cbuffer CameraInfo : register(b3)
{
    float3 cameraPos;
    float cameraPadding;
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

// Simple directional light for cube map rendering
static const float3 lightDir = normalize(float3(-1.0f, -1.0f, 1.0f));
static const float3 lightColor = float3(1.0f, 0.95f, 0.9f);
static const float ambientStrength = 0.3f;

float4 main(PS_INPUT input) : SV_Target
{
    // Sample texture
    float3 texColor = shaderTexture.Sample(samplerState, input.uv).rgb;
    float3 diffuseColor = texColor * materialDiffuse;
    
    // Normalize the normal
    float3 normal = normalize(input.normal);
    
    // Calculate ambient
    float3 ambient = ambientStrength * diffuseColor;
    
    // Calculate diffuse lighting (simple Lambertian)
    float NdotL = max(dot(normal, -lightDir), 0.0f);
    float3 diffuse = NdotL * lightColor * diffuseColor;
  
    // Calculate specular (Blinn-Phong)
    float3 viewDir = normalize(cameraPos - input.worldPos);
    float3 halfDir = normalize(-lightDir + viewDir);
    float NdotH = max(dot(normal, halfDir), 0.0f);
    float spec = pow(NdotH, max(specularPower, 1.0f));
    float3 specular = spec * lightColor * materialSpecular * 0.5f;
  
    // Combine
    float3 finalColor = ambient + diffuse + specular;
    
    return float4(finalColor, 1.0f);
}
