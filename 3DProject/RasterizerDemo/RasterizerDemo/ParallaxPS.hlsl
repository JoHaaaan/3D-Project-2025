// PARALLAX OCCLUSION MAPPING PIXEL SHADER (NO NORMAL MAP)

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
Texture2D heightTexture : register(t1);
SamplerState samplerState : register(s0);

static const float HEIGHT_SCALE = 0.03f;
static const float MIN_LAYERS = 8.0f;
static const float MAX_LAYERS = 64.0f;
static const float DEPTH_BIAS = 0.002f;

float3x3 ComputeTBN(float3 worldPosition, float3 worldNormal, float2 uv)
{
    float3 dp1 = ddx(worldPosition);
    float3 dp2 = ddy(worldPosition);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    float invDet = 1.0f / (abs(det) + 0.0001f);
    
    float3 tangent = (dp1 * duv2.y - dp2 * duv1.y) * invDet;
    float3 bitangent = (dp2 * duv1.x - dp1 * duv2.x) * invDet;
    
    if (det < 0.0f)
        tangent = -tangent;
    
    float3 N = normalize(worldNormal);
    tangent = tangent - N * dot(N, tangent);
    tangent = normalize(tangent);
    
    bitangent = cross(N, tangent);
    bitangent = normalize(bitangent);
    if (det < 0.0f)
        bitangent = -bitangent;
    
    return float3x3(tangent, bitangent, N);
}

float2 ParallaxOcclusionMapping(float2 texCoords, float3 viewDirTangent, float2 gradientX, float2 gradientY, out float parallaxHeight)
{
    viewDirTangent = normalize(viewDirTangent);
    float viewZ = abs(viewDirTangent.z);
    
    float numLayers = lerp(MAX_LAYERS, MIN_LAYERS, viewZ);
    float layerDepth = 1.0f / numLayers;
    float currentLayerDepth = 0.0f;
    
    float2 P = viewDirTangent.xy * HEIGHT_SCALE;
    float2 deltaTexCoords = P / numLayers;
  
    float2 currentTexCoords = texCoords;
    
    // Extracting height from Red (.r) channel
    float currentDepthMapValue = heightTexture.SampleGrad(samplerState, currentTexCoords, gradientX, gradientY).r;

    [loop]
    for (int i = 0; i < 64 && currentLayerDepth < currentDepthMapValue; i++)
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = heightTexture.SampleGrad(samplerState, currentTexCoords, gradientX, gradientY).r;
        currentLayerDepth += layerDepth;
    }
    
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = (heightTexture.SampleGrad(samplerState, prevTexCoords, gradientX, gradientY).r) - currentLayerDepth + layerDepth;
    
    float weight = afterDepth / (afterDepth - beforeDepth + 0.0001f);
    weight = saturate(weight);
    float2 finalTexCoords = lerp(currentTexCoords, prevTexCoords, weight);

    float finalHeight = lerp(currentDepthMapValue, heightTexture.SampleGrad(samplerState, prevTexCoords, gradientX, gradientY).r, weight);
    parallaxHeight = finalHeight;

    return finalTexCoords;
}

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
  
    float3 normalizedNormal = normalize(input.worldNormal);
    float2 gradientX = ddx(input.uv);
    float2 gradientY = ddy(input.uv);
    
    float3x3 TBN = ComputeTBN(input.worldPosition, normalizedNormal, input.uv);
    float3 viewDirWorld = normalize(cameraPosition - input.worldPosition);
    
    float3 viewDirTangent;
    viewDirTangent.x = dot(TBN[0], viewDirWorld);
    viewDirTangent.y = dot(TBN[1], viewDirWorld);
    viewDirTangent.z = dot(TBN[2], viewDirWorld);
  
    float parallaxHeight = 0.0f;
    float2 parallaxUV = ParallaxOcclusionMapping(input.uv, viewDirTangent, gradientX, gradientY, parallaxHeight);
  
    // Clip out-of-bounds UVs to fix texture wrapping at edges
    if (parallaxUV.x < 0.0f || parallaxUV.x > 1.0f || parallaxUV.y < 0.0f || parallaxUV.y > 1.0f)
    {
        discard;
    }

    float3 texColor = diffuseTexture.SampleGrad(samplerState, parallaxUV, gradientX, gradientY).rgb;
    float3 diffuseColor = texColor * materialDiffuse;

    // Direct assignment - No normal map calculation
    float3 worldNormal = normalizedNormal;
  
    float depthFactor = parallaxHeight * parallaxHeight;
    float depthOffset = depthFactor * HEIGHT_SCALE + DEPTH_BIAS;
    float3 adjustedWorldPosition = input.worldPosition - normalizedNormal * depthOffset;

    float ambientStrength = saturate(dot(materialAmbient, float3(0.333f, 0.333f, 0.333f)));
    float specularStrength = saturate(dot(materialSpecular, float3(0.333f, 0.333f, 0.333f)));
    float specularPacked = saturate(specularPower / 256.0f);

    output.Albedo = float4(diffuseColor, ambientStrength);
    output.Normal = float4(worldNormal * 0.5f + 0.5f, specularStrength);
    output.Extra = float4(adjustedWorldPosition, specularPacked);

    return output;
}