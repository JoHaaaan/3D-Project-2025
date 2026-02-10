// PARALLAX OCCLUSION MAPPING PIXEL SHADER
// Uses pre-computed TBN matrix from vertex shader
// Key technique: TBN basis computed in vertex shader using Gram-Schmidt orthogonalization

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
    float3 worldTangent : TANGENT;
    float3 worldBitangent : BITANGENT;
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
static const float MAX_LAYERS = 128.0f;

float2 ParallaxOcclusionMapping(float2 texCoords, float3 viewDirTangent, float2 gradientX, float2 gradientY, out float parallaxHeight)
{
    float numLayers = lerp(MAX_LAYERS, MIN_LAYERS, abs(dot(float3(0.0f, 0.0f, 1.0f), viewDirTangent)));
    float layerDepth = 1.0f / numLayers;
    float currentLayerDepth = 0.0f;
    
    float2 P = viewDirTangent.xy / max(viewDirTangent.z, 0.05f) * HEIGHT_SCALE;
    
    float offsetLength = length(P);
    if (offsetLength > HEIGHT_SCALE)
    {
        P = normalize(P) * HEIGHT_SCALE;
    }
    
    float2 deltaTexCoords = P / numLayers;
  
    float2 currentTexCoords = texCoords;
    float currentDepthMapValue = normalHeightTexture.SampleGrad(samplerState, currentTexCoords, gradientX, gradientY).a;

    [unroll(64)]
    for (int i = 0; i < 64; i++)
    {
        if (currentLayerDepth >= currentDepthMapValue || i >= (int) numLayers)
            break;

        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = normalHeightTexture.SampleGrad(samplerState, currentTexCoords, gradientX, gradientY).a;
        currentLayerDepth += layerDepth;
    }
    
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = normalHeightTexture.SampleGrad(samplerState, prevTexCoords, gradientX, gradientY).a - currentLayerDepth + layerDepth;
    
    float weight = afterDepth / (afterDepth - beforeDepth);
    float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0f - weight);

    parallaxHeight = currentLayerDepth;

    return finalTexCoords;
}

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
  
    float2 gradientX = ddx(input.uv);
    float2 gradientY = ddy(input.uv);
    
    // Normalize TBN basis vectors (interpolation may have denormalized them)
    float3 T = normalize(input.worldTangent);
    float3 B = normalize(input.worldBitangent);
    float3 N = normalize(input.worldNormal);
    
    // Build TBN matrix from pre-computed vertex shader values
    float3x3 TBN = float3x3(T, B, N);
    
    float3 viewDirWorld = normalize(cameraPosition - input.worldPosition);
    float3 viewDirTangent = normalize(mul(transpose(TBN), viewDirWorld));
  
    float parallaxHeight = 0.0f;
    float2 parallaxUV = ParallaxOcclusionMapping(input.uv, viewDirTangent, gradientX, gradientY, parallaxHeight);
  
    float3 texColor = diffuseTexture.SampleGrad(samplerState, parallaxUV, gradientX, gradientY).rgb;
    float3 diffuseColor = texColor * materialDiffuse;
    
    float3 normalMapSample = normalHeightTexture.SampleGrad(samplerState, parallaxUV, gradientX, gradientY).rgb;
    float3 tangentNormal = normalMapSample * 2.0f - 1.0f;
  
    float3 worldNormal = normalize(mul(tangentNormal, TBN));

    float heightValue = normalHeightTexture.SampleGrad(samplerState, parallaxUV, gradientX, gradientY).a;
    float3 adjustedWorldPosition = input.worldPosition + N * (heightValue - 0.5f) * HEIGHT_SCALE * 2.0f;

    float ambientStrength = saturate(dot(materialAmbient, float3(0.333f, 0.333f, 0.333f)));
    float specularStrength = saturate(dot(materialSpecular, float3(0.333f, 0.333f, 0.333f)));
    float specularPacked = saturate(specularPower / 256.0f);

    output.Albedo = float4(diffuseColor, ambientStrength);
    output.Normal = float4(worldNormal * 0.5f + 0.5f, specularStrength);
    output.Extra = float4(adjustedWorldPosition, specularPacked);

    return output;
}