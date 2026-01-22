// ========================================
// DEFERRED LIGHTING COMPUTE SHADER
// ========================================
// Part 2 of 2: Deferred Rendering Pipeline (Geometry Pass -> Lighting Pass)
// Reads G-Buffer and computes lighting for all lights in a single pass
// Key techniques: Compute shader parallelism, multi-light support, shadow mapping

// Light Data Structure
struct LightData
{
    float4x4 viewProj;
    float3 position;
    float intensity;
    float3 direction;
    float range;
    float3 color;
    float spotAngle;
    int type;
    int enabled;
    float2 padding;
};

// Constant Buffers
cbuffer CameraBuffer : register(b2)
{
    float3 cameraPosition;
    float padding_Camera;
};

cbuffer LightingToggleBuffer : register(b4)
{
    int showAlbedoOnly;
    int enableDiffuse;
    int enableSpecular;
    int padding_Toggle;
};

// G-Buffer input textures (from geometry pass)
Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
Texture2D gWorldPos : register(t2);
Texture2DArray shadowMaps : register(t3);
StructuredBuffer<LightData> lights : register(t4);

RWTexture2D<float4> outColor : register(u0);

SamplerComparisonState shadowSampler : register(s1);

// Spotlight cone attenuation calculation
float CalculateSpotlight(float3 lightDir, float3 spotDirection, float spotAngle)
{
    float cosAngle = dot(-lightDir, normalize(spotDirection));
    float cosInner = cos(spotAngle * 0.5f);
    float cosOuter = cos(spotAngle);
    
    float epsilon = cosInner - cosOuter;
    float attenuation = saturate((cosAngle - cosOuter) / epsilon);
    
    return attenuation * attenuation;
}

// PCF shadow mapping with single sample
float CalculateShadow(float3 worldPosition, float4x4 lightViewProj, uint lightIndex)
{
    // Transform world position to light's clip space
    float4 lightSpacePosition = mul(float4(worldPosition, 1.0f), lightViewProj);
 
    lightSpacePosition.xyz /= lightSpacePosition.w;
    
    // Convert to shadow map texture coordinates
    float2 shadowUV;
    shadowUV.x = lightSpacePosition.x * 0.5f + 0.5f;
    shadowUV.y = -lightSpacePosition.y * 0.5f + 0.5f;
    
    // Bounds check
    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f || shadowUV.y < 0.0f || shadowUV.y > 1.0f)
        return 1.0f;
    
    float depth = lightSpacePosition.z;
    
    if (depth < 0.0f || depth > 1.0f)
        return 1.0f;
    
    // Sample shadow map with comparison (returns 0 if in shadow, 1 if lit)
    float shadow = shadowMaps.SampleCmpLevelZero(shadowSampler, float3(shadowUV, lightIndex), depth);
    
    return shadow;
}

// Compute shader processes screen pixels in parallel (16x16 thread groups)
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 pixel = DTid.xy;
    
    uint width, height;
    gAlbedo.GetDimensions(width, height);
    if (pixel.x >= width || pixel.y >= height)
        return;
  
    // Unpack G-Buffer data
    float4 albedoSample = gAlbedo.Load(int3(pixel, 0));
    float3 diffuseColor = albedoSample.rgb;
    float ambientStrength = albedoSample.a;
    
    float4 normalSample = gNormal.Load(int3(pixel, 0));
    float3 normalPacked = normalSample.rgb;
    float specularStrength = normalSample.a;
    
    float4 positionSample = gWorldPos.Load(int3(pixel, 0));
    float3 worldPosition = positionSample.xyz;
    float specularPacked = positionSample.w;
    
    // Reconstruct world-space normal and material properties
    float3 normal = normalize(normalPacked * 2.0f - 1.0f);
    float specularPower = max(specularPacked * 256.0f, 1.0f);
    
    float3 materialDiffuse = diffuseColor;
    float3 materialAmbient = ambientStrength * diffuseColor * 0.2f;
    float3 materialSpecular = specularStrength * float3(1.0f, 1.0f, 1.0f);
  
    // Debug visualization mode
    if (showAlbedoOnly != 0)
    {
        outColor[pixel] = float4(diffuseColor, 1.0f);
        return;
    }
    
    // Lighting accumulation (Blinn-Phong model)
    float3 viewDirection = normalize(cameraPosition - worldPosition);
    float3 lighting = materialAmbient;
    
    // Iterate through all lights and accumulate contributions
    uint numLights, stride;
    lights.GetDimensions(numLights, stride);
    
    for (uint i = 0; i < numLights; ++i)
    {
        LightData light = lights[i];
        
        if (light.enabled == 0)
            continue;
        
        float3 lightDirection;
        float attenuation = 1.0f;
    
        if (light.type == 0) // Directional Light
        {
            lightDirection = normalize(-light.direction);
        }
        else if (light.type == 1) // Spotlight
        {
            float3 toLight = light.position - worldPosition;
            float distance = length(toLight);
            lightDirection = toLight / distance;
            
            // Distance attenuation (quadratic falloff)
            attenuation = saturate(1.0f - (distance / light.range));
            attenuation *= attenuation;
  
            // Spotlight cone attenuation
            float spotEffect = CalculateSpotlight(lightDirection, light.direction, light.spotAngle);
            attenuation *= spotEffect;
        }
        
        if (attenuation < 0.001f)
            continue;
        
        // Shadow mapping
        float shadow = CalculateShadow(worldPosition, light.viewProj, i);
        
        // Blinn-Phong lighting model
        float3 halfVector = normalize(lightDirection + viewDirection);
        
        // Diffuse (Lambertian)
        if (enableDiffuse != 0)
        {
            float diffuseFactor = max(dot(normal, lightDirection), 0.0f);
            float3 diffuse = diffuseFactor * light.intensity * light.color * materialDiffuse;
            lighting += diffuse * attenuation * shadow;
        }
    
        // Specular (Blinn-Phong)
        if (enableSpecular != 0)
        {
            float specularAngle = max(dot(normal, halfVector), 0.0f);
            float specularFactor = pow(specularAngle, specularPower);
            float3 specular = specularFactor * light.intensity * light.color * materialSpecular;
            lighting += specular * attenuation * shadow;
        }
    }
    
    lighting = saturate(lighting);
    outColor[pixel] = float4(lighting, 1.0f);
}
