// Parallax Occlusion Mapping Pixel Shader
// Uses height data from alpha channel to perform ray marching for depth parallax

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

struct PS_OUTPUT
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Extra : SV_Target2;
};

Texture2D diffuseTexture : register(t0);
Texture2D normalHeightTexture : register(t1);
SamplerState samplerState : register(s0);

// Parallax Occlusion Mapping parameters
static const float HEIGHT_SCALE = 0.08f;
static const float MIN_LAYERS = 16.0f;
static const float MAX_LAYERS = 64.0f;

// Compute TBN matrix using screen-space derivatives
float3x3 ComputeTBN(float3 worldPosition, float3 worldNormal, float2 uv)
{
    // Get edge vectors of the pixel triangle
    float3 dp1 = ddx(worldPosition);
    float3 dp2 = ddy(worldPosition);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
    
    // Solve the linear system
    float3 dp2perp = cross(dp2, worldNormal);
    float3 dp1perp = cross(worldNormal, dp1);
    
    float3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
    
    // Construct a scale-invariant frame
    float invmax = rsqrt(max(dot(tangent, tangent), dot(bitangent, bitangent)));
    
    return float3x3(tangent * invmax, bitangent * invmax, worldNormal);
}

// Parallax Occlusion Mapping with ray marching
float2 ParallaxOcclusionMapping(float2 texCoords, float3 viewDirTangent, float2 gradientX, float2 gradientY)
{
    // Number of depth layers (adaptive based on view angle)
    float numLayers = lerp(MAX_LAYERS, MIN_LAYERS, abs(dot(float3(0.0f, 0.0f, 1.0f), viewDirTangent)));
    
    // Calculate the size of each layer
    float layerDepth = 1.0f / numLayers;
    
    // Depth of current layer
    float currentLayerDepth = 0.0f;
    
    // The amount to shift the texture coordinates per layer
    float2 parallaxOffset = viewDirTangent.xy * HEIGHT_SCALE;
    float2 deltaTexCoords = parallaxOffset / numLayers;
  
    // Get initial values
    float2 currentTexCoords = texCoords;
    float currentDepthMapValue = normalHeightTexture.SampleGrad(samplerState, currentTexCoords, gradientX, gradientY).a;
    
    // Ray march through the height field
    [unroll(64)]
    for (int i = 0; i < 64; i++)
    {
    // Break if we've gone deep enough or exceeded actual layer count
        if (currentLayerDepth >= currentDepthMapValue || i >= (int)numLayers)
      break;
    
        // Shift texture coordinates along direction of parallax
   currentTexCoords -= deltaTexCoords;
      
        // Get depth map value at current texture coordinates
     currentDepthMapValue = normalHeightTexture.SampleGrad(samplerState, currentTexCoords, gradientX, gradientY).a;
  
  // Get depth of next layer
        currentLayerDepth += layerDepth;
    }
    
    // Parallax Occlusion Mapping interpolation (smoothing)
    
    // Get texture coordinates before collision
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;
  
    // Get depth after and before collision for linear interpolation
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = normalHeightTexture.SampleGrad(samplerState, prevTexCoords, gradientX, gradientY).a - currentLayerDepth + layerDepth;
  
    // Interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0f - weight);
    
    return finalTexCoords;
}

PS_OUTPUT main(PS_INPUT input)
{
  PS_OUTPUT output;
    
    // Normalize the vertex normal
    float3 normalizedNormal = normalize(input.worldNormal);
    
    // Calculate texture gradients before the loop
    float2 gradientX = ddx(input.uv);
    float2 gradientY = ddy(input.uv);
    
  // Compute TBN matrix using partial derivatives
  float3x3 TBN = ComputeTBN(input.worldPosition, normalizedNormal, input.uv);
    
    // Calculate view direction in world space
    float3 viewDirWorld = normalize(cameraPosition - input.worldPosition);
 
    // Transform view direction to tangent space
    float3 viewDirTangent = normalize(mul(transpose(TBN), viewDirWorld));
  
    // Perform Parallax Occlusion Mapping to get displaced UVs
    float2 parallaxUV = ParallaxOcclusionMapping(input.uv, viewDirTangent, gradientX, gradientY);
 
 // Sample textures with the displaced UVs
    float3 texColor = diffuseTexture.SampleGrad(samplerState, parallaxUV, gradientX, gradientY).rgb;
    float3 diffuseColor = texColor * materialDiffuse;
    
    // Sample normal map with displaced UVs
    float3 normalMapSample = normalHeightTexture.SampleGrad(samplerState, parallaxUV, gradientX, gradientY).rgb;
    
    // Convert from [0,1] to [-1,1] range
    float3 tangentNormal = normalMapSample * 2.0f - 1.0f;
    
    // Transform normal from tangent space to world space
    float3 worldNormal = normalize(mul(tangentNormal, TBN));

    // Pack material properties
    float ambientStrength = saturate(dot(materialAmbient, float3(0.333f, 0.333f, 0.333f)));
    float specularStrength = saturate(dot(materialSpecular, float3(0.333f, 0.333f, 0.333f)));
    float specularPacked = saturate(specularPower / 256.0f);
    
    // RT0: Albedo + ambient strength
    output.Albedo = float4(diffuseColor, ambientStrength);
    
    // RT1: Packed normal + specular strength
    output.Normal = float4(worldNormal * 0.5f + 0.5f, specularStrength);

  // RT2: World position + shininess
    output.Extra = float4(input.worldPosition, specularPacked);

    return output;
}
