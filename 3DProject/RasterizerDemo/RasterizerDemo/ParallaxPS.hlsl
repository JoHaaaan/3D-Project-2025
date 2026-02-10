// PARALLAX OCCLUSION MAPPING PIXEL SHADER
// Fixed: Improved TBN stability and view vector calculation to prevent warping

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

static const float HEIGHT_SCALE = 0.08f;
static const float MIN_LAYERS = 8.0f;
static const float MAX_LAYERS = 64.0f;

float3x3 ComputeTBN(float3 worldPosition, float3 worldNormal, float2 uv)
{
    // Get screen-space derivatives
    float3 dp1 = ddx(worldPosition);
    float3 dp2 = ddy(worldPosition);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
    
    // Solve the linear system to get tangent and bitangent
    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    
    // Avoid division by zero with a small epsilon
    float invDet = 1.0f / (abs(det) + 0.0001f);
    
    // Calculate tangent and bitangent
    float3 tangent = (dp1 * duv2.y - dp2 * duv1.y) * invDet;
    float3 bitangent = (dp2 * duv1.x - dp1 * duv2.x) * invDet;
    
    // Handle UV winding (handedness)
    if (det < 0.0f)
    {
        tangent = -tangent;
    }
    
    // Normalize the interpolated normal
    float3 N = normalize(worldNormal);
    
    // Gram-Schmidt orthonormalization
    tangent = tangent - N * dot(N, tangent);
    tangent = normalize(tangent);
    
    // Recompute bitangent to ensure orthogonality
    bitangent = cross(N, tangent);
    bitangent = normalize(bitangent);
    
    // Correct bitangent direction based on UV handedness
    if (det < 0.0f)
    {
        bitangent = -bitangent;
    }
    
    return float3x3(tangent, bitangent, N);
}

float2 ParallaxOcclusionMapping(float2 texCoords, float3 viewDirTangent, float2 gradientX, float2 gradientY, out float parallaxHeight)
{
    viewDirTangent = normalize(viewDirTangent);
    
    // Ensure viewDirTangent.z is positive (looking into the surface)
    float viewZ = abs(viewDirTangent.z);
    
    // Adaptive layer count based on view angle
    float numLayers = lerp(MAX_LAYERS, MIN_LAYERS, viewZ);
    float layerDepth = 1.0f / numLayers;
    float currentLayerDepth = 0.0f;
    
    // Calculate texture offset per layer - simplified scaling
    float2 P = viewDirTangent.xy * HEIGHT_SCALE;
    float2 deltaTexCoords = P / numLayers;
  
    float2 currentTexCoords = texCoords;
    float currentDepthMapValue = normalHeightTexture.SampleGrad(samplerState, currentTexCoords, gradientX, gradientY).a;

    // Ray marching loop
    [loop]
    for (int i = 0; i < 64 && currentLayerDepth < currentDepthMapValue; i++)
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = normalHeightTexture.SampleGrad(samplerState, currentTexCoords, gradientX, gradientY).a;
        currentLayerDepth += layerDepth;
    }
    
    // Binary refinement for more accurate intersection
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = normalHeightTexture.SampleGrad(samplerState, prevTexCoords, gradientX, gradientY).a - currentLayerDepth + layerDepth;
    
    // Linear interpolation for final position
    float weight = afterDepth / (afterDepth - beforeDepth + 0.0001f);
    weight = saturate(weight);
    float2 finalTexCoords = lerp(currentTexCoords, prevTexCoords, weight);

    // Output the interpolated height for world position adjustment
    float finalHeight = lerp(currentDepthMapValue, normalHeightTexture.SampleGrad(samplerState, prevTexCoords, gradientX, gradientY).a, weight);
    parallaxHeight = finalHeight;

    return finalTexCoords;
}

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
  
    float3 normalizedNormal = normalize(input.worldNormal);

    // Compute gradients BEFORE any UV modification
    float2 gradientX = ddx(input.uv);
    float2 gradientY = ddy(input.uv);
    
    // Build orthonormal TBN matrix
    float3x3 TBN = ComputeTBN(input.worldPosition, normalizedNormal, input.uv);
    
    // Transform view direction to tangent space
    float3 viewDirWorld = normalize(cameraPosition - input.worldPosition);
    
    // TBN matrix rows are T, B, N - transpose to transform world->tangent
    float3 viewDirTangent;
    viewDirTangent.x = dot(TBN[0], viewDirWorld);
    viewDirTangent.y = dot(TBN[1], viewDirWorld);
    viewDirTangent.z = dot(TBN[2], viewDirWorld);
  
    float parallaxHeight = 0.0f;
    float2 parallaxUV = ParallaxOcclusionMapping(input.uv, viewDirTangent, gradientX, gradientY, parallaxHeight);
  
    // Sample textures with original gradients
    float3 texColor = diffuseTexture.SampleGrad(samplerState, parallaxUV, gradientX, gradientY).rgb;
    float3 diffuseColor = texColor * materialDiffuse;
    
    float3 normalMapSample = normalHeightTexture.SampleGrad(samplerState, parallaxUV, gradientX, gradientY).rgb;
    float3 tangentNormal = normalMapSample * 2.0f - 1.0f;
  
    // Transform normal from tangent space to world space
    float3 worldNormal;
    worldNormal = tangentNormal.x * TBN[0] + tangentNormal.y * TBN[1] + tangentNormal.z * TBN[2];
    worldNormal = normalize(worldNormal);

    // Adjust world position based on parallax depth
    float depthOffset = (1.0f - parallaxHeight) * HEIGHT_SCALE;
    float3 adjustedWorldPosition = input.worldPosition - normalizedNormal * depthOffset;

    float ambientStrength = saturate(dot(materialAmbient, float3(0.333f, 0.333f, 0.333f)));
    float specularStrength = saturate(dot(materialSpecular, float3(0.333f, 0.333f, 0.333f)));
    float specularPacked = saturate(specularPower / 256.0f);

    output.Albedo = float4(diffuseColor, ambientStrength);
    output.Normal = float4(worldNormal * 0.5f + 0.5f, specularStrength);
    output.Extra = float4(adjustedWorldPosition, specularPacked);

    return output;
}