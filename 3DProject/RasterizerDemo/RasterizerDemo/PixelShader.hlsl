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
    float4 position : SV_POSITION;    // Clip-space position
    float3 worldPos : WORLD_POSITION; // World-space position
    float3 normal : NORMAL;           // World-space normal
    float2 uv : TEXCOORD0;            // Texture coordinates
};

Texture2D shaderTexture : register(t0);
SamplerState samplerState : register(s0);

float4 main(PS_INPUT input) : SV_TARGET
{
    // Normalize vectors
    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(lightPosition - input.worldPos);
    float3 viewDir = normalize(cameraPosition - input.worldPos);
    float3 reflectDir = reflect(-lightDir, normal);

    // Ambient lighting
    float3 ambient = lightIntensity * lightColor * materialAmbient;

    // Diffuse lighting
    float diffuseFactor = max(dot(normal, lightDir), 0.0);
    float3 diffuse = diffuseFactor * lightIntensity * lightColor * materialDiffuse;

    // Specular lighting
    float specAngle = max(dot(viewDir, reflectDir), 0.0);
    float specularFactor = pow(specAngle, specularPower);
    float3 specular = specularFactor * lightIntensity * lightColor * materialSpecular;

    // Sample texture
    float4 textureColor = shaderTexture.Sample(samplerState, input.uv);

    // Combine lighting components with the texture color
    float3 finalColor = (ambient + diffuse + specular) * textureColor.rgb;

    return float4(finalColor, 1.0f);
}

