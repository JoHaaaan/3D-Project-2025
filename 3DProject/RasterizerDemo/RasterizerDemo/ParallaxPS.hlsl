// PARALLAX OCCLUSION MAPPING PIXEL SHADER
// Corrected: Implements Z-division with offset clamping to prevent warping

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

// POM tuning parameters
static const float HEIGHT_SCALE = 0.05f;
static const float MIN_LAYERS = 8.0f;
static const float MAX_LAYERS = 32.0f;

// Constructs tangent-space basis using screen-space derivatives
float3x3 ComputeTBN(float3 worldPosition, float3 worldNormal, float2 uv)
{
    float3 dp1 = ddx(worldPosition);
    float3 dp2 = ddy(worldPosition);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
    
    float3 dp2perp = cross(dp2, worldNormal);
    float3 dp1perp = cross(worldNormal, dp1);
    
    float3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;

    // Flip bitangent to align with DirectX texture coordinate system
    bitangent = bitangent;

    float invmax = rsqrt(max(dot(tangent, tangent), dot(bitangent, bitangent)));
    
    return float3x3(tangent * invmax, bitangent * invmax, worldNormal);
}

// Ray marches through height field to find surface intersection
float2 ParallaxOcclusionMapping(float2 texCoords, float3 viewDirTangent, float2 gradientX, float2 gradientY)
{
    // Adaptive sampling based on viewing angle
    float numLayers = lerp(MAX_LAYERS, MIN_LAYERS, abs(dot(float3(0.0f, 0.0f, 1.0f), viewDirTangent)));
    float layerDepth = 1.0f / numLayers;
    float currentLayerDepth = 0.0f;
    

    
    float2 P = viewDirTangent.xy / max(viewDirTangent.z, 0.05f) * HEIGHT_SCALE;
    
    // Clamp the maximum offset to prevent extreme warping at grazing angles
    float offsetLength = length(P);
    if (offsetLength > HEIGHT_SCALE)
    {
        P = normalize(P) * HEIGHT_SCALE;
    }
    
    float2 deltaTexCoords = P / numLayers;
  
    float2 currentTexCoords = texCoords;
    float currentDepthMapValue = normalHeightTexture.SampleGrad(samplerState, currentTexCoords, gradientX, gradientY).a;

    // Ray march loop
    [unroll(64)]
    for (int i = 0; i < 64; i++)
    {
        if (currentLayerDepth >= currentDepthMapValue || i >= (int) numLayers)
            break;

        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = normalHeightTexture.SampleGrad(samplerState, currentTexCoords, gradientX, gradientY).a;
        currentLayerDepth += layerDepth;
    }
    
    // POM Interpolation
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = normalHeightTexture.SampleGrad(samplerState, prevTexCoords, gradientX, gradientY).a - currentLayerDepth + layerDepth;
    
    float weight = afterDepth / (afterDepth - beforeDepth);
    float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0f - weight);

    return finalTexCoords;
}

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
    
    float3 normalizedNormal = normalize(input.worldNormal);

    // Calculate gradients before flow control to prevent mip-map artifacts
    float2 gradientX = ddx(input.uv);
    float2 gradientY = ddy(input.uv);
    
    float3x3 TBN = ComputeTBN(input.worldPosition, normalizedNormal, input.uv);
    
    float3 viewDirWorld = normalize(cameraPosition - input.worldPosition);
    float3 viewDirTangent = normalize(mul(transpose(TBN), viewDirWorld));
  
    // Perform POM
    float2 parallaxUV = ParallaxOcclusionMapping(input.uv, viewDirTangent, gradientX, gradientY);
    
    // Using SampleGrad is essential to prevent "sparkling" artifacts on the warped UVs
    float3 texColor = diffuseTexture.SampleGrad(samplerState, parallaxUV, gradientX, gradientY).rgb;
    float3 diffuseColor = texColor * materialDiffuse;
    
    float3 normalMapSample = normalHeightTexture.SampleGrad(samplerState, parallaxUV, gradientX, gradientY).rgb;
    float3 tangentNormal = normalMapSample * 2.0f - 1.0f;
  
    float3 worldNormal = normalize(mul(tangentNormal, TBN));

    // Simple lighting data packing
    float ambientStrength = saturate(dot(materialAmbient, float3(0.333f, 0.333f, 0.333f)));
    float specularStrength = saturate(dot(materialSpecular, float3(0.333f, 0.333f, 0.333f)));
    float specularPacked = saturate(specularPower / 256.0f);

    output.Albedo = float4(diffuseColor, ambientStrength);
    output.Normal = float4(worldNormal * 0.5f + 0.5f, specularStrength);
    output.Extra = float4(input.worldPosition, specularPacked);

    return output;
}