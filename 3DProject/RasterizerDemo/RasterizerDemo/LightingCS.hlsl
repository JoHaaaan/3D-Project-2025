// LightingCS.hlsl - Multi-Light Deferred Rendering (No Shadows)

// ==== Light Data Structure ====
struct LightData
{
    float4x4 viewProj;      // Light's view-projection matrix (for future shadow use)
    float3 position;        // Light position
    float intensity;        // Light intensity
    float3 direction;       // Light direction (for spotlights)
    float range;            // Light range
    float3 color;           // Light color
    float spotAngle;        // Spotlight cone angle
    int type;               // 0 = Directional, 1 = Spot
    int enabled;            // 1 = enabled, 0 = disabled
    float2 padding;         // Alignment padding
};

// ==== Constant Buffers ====
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
    int paddingToggle;
};

// ==== Resources ====
Texture2D gAlbedo : register(t0);           // Albedo + ambient strength
Texture2D gNormal : register(t1);           // Normal + specular strength
Texture2D gWorldPos : register(t2);         // World position + shininess
Texture2DArray shadowMaps : register(t3);   // Shadow depth maps (NEW!)

StructuredBuffer<LightData> lights : register(t4);  // Light data

RWTexture2D<float4> outColor : register(u0);

// ==== Shadow Sampler ====
SamplerComparisonState shadowSampler : register(s1);

// ==== Spotlight Attenuation ====
float CalculateSpotlight(float3 lightDir, float3 spotDirection, float spotAngle)
{
    float cosAngle = dot(-lightDir, normalize(spotDirection));
    float cosInner = cos(spotAngle * 0.5f);
    float cosOuter = cos(spotAngle);
    
    // Smooth falloff from inner to outer cone
    float epsilon = cosInner - cosOuter;
    float attenuation = saturate((cosAngle - cosOuter) / epsilon);
    
    return attenuation * attenuation; // Square for smoother falloff
}

// ==== Shadow Calculation ====
float CalculateShadow(float3 worldPos, float4x4 lightViewProj, uint lightIndex)
{
    // Transform world position to light's clip space
    float4 lightSpacePos = mul(float4(worldPos, 1.0f), lightViewProj);
    
    // Perspective divide
    lightSpacePos.xyz /= lightSpacePos.w;
    
    // Convert to texture coordinates [0,1]
    float2 shadowUV;
    shadowUV.x = lightSpacePos.x * 0.5f + 0.5f;
    shadowUV.y = -lightSpacePos.y * 0.5f + 0.5f;
    
    // Check if position is within shadow map bounds
    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f || shadowUV.y < 0.0f || shadowUV.y > 1.0f)
  return 1.0f; // Outside shadow map = fully lit
    
    float depth = lightSpacePos.z;
    
    // Check if depth is valid
    if (depth < 0.0f || depth > 1.0f)
        return 1.0f;
  
    // Sample shadow map with PCF (Percentage Closer Filtering)
    // SampleCmpLevelZero does hardware PCF and returns 0 (in shadow) or 1 (lit)
 float shadow = shadowMaps.SampleCmpLevelZero(shadowSampler, float3(shadowUV, lightIndex), depth);
    
    return shadow;
}

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 pixel = DTid.xy;
    
    uint w, h;
    gAlbedo.GetDimensions(w, h);
    if (pixel.x >= w || pixel.y >= h)
        return;
    
    // ==== Read G-Buffer ====
    float4 albedoSample = gAlbedo.Load(int3(pixel, 0));
    float3 diffuseColor = albedoSample.rgb;
    float ambientStrength = albedoSample.a;
    
    float4 normalSample = gNormal.Load(int3(pixel, 0));
    float3 nPacked = normalSample.rgb;
    float specularStrength = normalSample.a;
    
    float4 posSample = gWorldPos.Load(int3(pixel, 0));
    float3 worldPos = posSample.xyz;
    float specPacked = posSample.w;
    
    float3 normal = normalize(nPacked * 2.0f - 1.0f);
    float specularPower = max(specPacked * 256.0f, 1.0f);
    
    // ==== Material Setup ====
    float3 materialDiffuse = diffuseColor;
    float3 materialAmbient = ambientStrength * diffuseColor * 0.2f;
    float3 materialSpecular = specularStrength * float3(1.0f, 1.0f, 1.0f);
    
    // ==== Debug Mode ====
    if (showAlbedoOnly != 0)
    {
        outColor[pixel] = float4(diffuseColor, 1.0f);
        return;
    }
    
    // ==== Lighting Accumulation ====
    float3 viewDir = normalize(cameraPosition - worldPos);
    float3 lighting = materialAmbient; // Base ambient
    
    // Iterate through all lights
    uint numLights, stride;
    lights.GetDimensions(numLights, stride);
    
    for (uint i = 0; i < numLights; ++i)
    {
        LightData light = lights[i];
        
        if (light.enabled == 0)
            continue;
        
        float3 lightDir;
        float attenuation = 1.0f;
        
        if (light.type == 0) // Directional Light
        {
            lightDir = normalize(-light.direction);
        }
        else if (light.type == 1) // Spotlight
        {
            float3 toLight = light.position - worldPos;
            float distance = length(toLight);
            lightDir = toLight / distance;
            
            // Distance attenuation
            attenuation = saturate(1.0f - (distance / light.range));
            attenuation *= attenuation;
            
            // Spotlight cone attenuation
            float spotEffect = CalculateSpotlight(lightDir, light.direction, light.spotAngle);
            attenuation *= spotEffect;
        }
        
        if (attenuation < 0.001f)
         continue;
        
   // Calculate shadow factor
  float shadow = CalculateShadow(worldPos, light.viewProj, i);
        
        // Blinn-Phong lighting
        float3 halfVec = normalize(lightDir + viewDir);
        
        // Diffuse
        if (enableDiffuse != 0)
     {
            float diffuseFactor = max(dot(normal, lightDir), 0.0f);
            float3 diffuse = diffuseFactor * light.intensity * light.color * materialDiffuse;
     lighting += diffuse * attenuation * shadow; // Apply shadow
   }
    
        // Specular
if (enableSpecular != 0)
        {
float specAngle = max(dot(normal, halfVec), 0.0f);
            float specularFactor = pow(specAngle, specularPower);
  float3 specular = specularFactor * light.intensity * light.color * materialSpecular;
      lighting += specular * attenuation * shadow; // Apply shadow
        }
    }
    
    lighting = saturate(lighting);
    outColor[pixel] = float4(lighting, 1.0f);
}
