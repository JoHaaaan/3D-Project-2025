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
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

Texture2D diffuseTexture : register(t0);
Texture2D normalHeightTexture : register(t1); // RGB = normal, A = height
SamplerState samplerState : register(s0);

struct PS_OUTPUT
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Extra : SV_Target2;
};

// Parallax Occlusion Mapping parameters
static const float heightScale = 0.08f;        // How "deep" the parallax effect is
static const float minLayers = 16.0f;   // Minimum ray marching steps
static const float maxLayers = 64.0f;      // Maximum ray marching steps

// Compute TBN matrix using screen-space derivatives
float3x3 ComputeTBN(float3 worldPos, float3 normal, float2 uv)
{
  // Get edge vectors of the pixel triangle
    float3 dp1 = ddx(worldPos);
    float3 dp2 = ddy(worldPos);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
    
    // Solve the linear system
  float3 dp2perp = cross(dp2, normal);
    float3 dp1perp = cross(normal, dp1);
    
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    
    // Construct a scale-invariant frame
    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    
    return float3x3(T * invmax, B * invmax, normal);
}

// Parallax Occlusion Mapping with ray marching
float2 ParallaxOcclusionMapping(float2 texCoords, float3 viewDirTangent, float2 dx, float2 dy)
{
    // Number of depth layers (adaptive based on view angle)
  float numLayers = lerp(maxLayers, minLayers, abs(dot(float3(0.0, 0.0, 1.0), viewDirTangent)));
    
    // Calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    
    // Depth of current layer
    float currentLayerDepth = 0.0;
    
    // The amount to shift the texture coordinates per layer (from vector P)
float2 P = viewDirTangent.xy * heightScale;
    float2 deltaTexCoords = P / numLayers;
    
    // Get initial values
    float2 currentTexCoords = texCoords;
    
  // Use SampleGrad to properly support mipmapping without aliasing
  float currentDepthMapValue = normalHeightTexture.SampleGrad(samplerState, currentTexCoords, dx, dy).a;
    
    // Ray march through the height field
 [unroll(64)]
    for(int i = 0; i < 64; i++)
    {
        // Break if we've gone deep enough or exceeded actual layer count
     if(currentLayerDepth >= currentDepthMapValue || i >= (int)numLayers)
     break;
    
        // Shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
      
        // Get depthmap value at current texture coordinates (use SampleGrad)
  currentDepthMapValue = normalHeightTexture.SampleGrad(samplerState, currentTexCoords, dx, dy).a;
  
 // Get depth of next layer
        currentLayerDepth += layerDepth;
    }
    
    // -- Parallax Occlusion Mapping interpolation (smoothing) --
  
  // Get texture coordinates before collision (reverse operations)
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;
  
    // Get depth after and before collision for linear interpolation
  float afterDepth = currentDepthMapValue - currentLayerDepth;
  float beforeDepth = normalHeightTexture.SampleGrad(samplerState, prevTexCoords, dx, dy).a - currentLayerDepth + layerDepth;
  
    // Interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
 float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);
    
    return finalTexCoords;
}

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT o;
    
    // Normalize the vertex normal
    float3 N = normalize(input.normal);
    
// *** CALCULATE TEXTURE GRADIENTS BEFORE THE LOOP ***
    // This prevents aliasing by allowing proper mipmap selection during ray marching
    float2 dx = ddx(input.uv);
    float2 dy = ddy(input.uv);
    
    // Compute TBN matrix using partial derivatives
    float3x3 TBN = ComputeTBN(input.worldPos, N, input.uv);
    
    // Calculate view direction in world space
    float3 viewDirWorld = normalize(cameraPosition - input.worldPos);
 
    // Transform view direction to tangent space
  float3 viewDirTangent = normalize(mul(transpose(TBN), viewDirWorld));
    
    // Perform Parallax Occlusion Mapping to get displaced UVs
    float2 parallaxUV = ParallaxOcclusionMapping(input.uv, viewDirTangent, dx, dy);
 
    // Sample textures with the displaced UVs - Use SampleGrad to ensure stable mipmapping
    float3 texColor = diffuseTexture.SampleGrad(samplerState, parallaxUV, dx, dy).rgb;
    float3 diffuseColor = texColor * materialDiffuse;
    
    // Sample normal map with displaced UVs - Use SampleGrad to ensure stable mipmapping
    float3 normalMapSample = normalHeightTexture.SampleGrad(samplerState, parallaxUV, dx, dy).rgb;
    
    // Convert from [0,1] to [-1,1] range
    float3 tangentNormal = normalMapSample * 2.0f - 1.0f;
    
    // Transform normal from tangent space to world space
    float3 worldNormal = normalize(mul(tangentNormal, TBN));
    
    // Pack material properties
    float ambientStrength = saturate(dot(materialAmbient, float3(0.333f, 0.333f, 0.333f)));
    float specularStrength = saturate(dot(materialSpecular, float3(0.333f, 0.333f, 0.333f)));
    float specPacked = saturate(specularPower / 256.0f);
    
    // RT0: diffuse color + ambient strength
    o.Albedo = float4(diffuseColor, ambientStrength);
    
    // RT1: packed normal (using the normal-mapped normal) + specular strength
    o.Normal = float4(worldNormal * 0.5f + 0.5f, specularStrength);

    // RT2: world position + shininess
    o.Extra = float4(input.worldPos, specPacked);

    return o;
}
