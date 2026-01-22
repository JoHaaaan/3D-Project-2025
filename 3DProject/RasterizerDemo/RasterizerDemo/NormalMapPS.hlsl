// ========================================
// NORMAL MAP PIXEL SHADER
// ========================================
// Demonstrates normal mapping with derivative-based TBN matrix construction
// Key technique: Screen-space derivatives for tangent-space basis without vertex attributes

cbuffer MaterialBuffer : register(b2)
{
    float3 materialAmbient;
    float padding1;
    float3 materialDiffuse;
    float padding2;
    float3 materialSpecular;
    float specularPower;
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
Texture2D normalMapTexture : register(t1);
SamplerState samplerState : register(s0);

// Constructs tangent-space basis (TBN) using screen-space derivatives
// This eliminates need for per-vertex tangent/bitangent attributes
float3x3 ComputeTBN(float3 worldPosition, float3 worldNormal, float2 uv)
{
    // Calculate position and UV deltas across the triangle
    float3 dp1 = ddx(worldPosition);
    float3 dp2 = ddy(worldPosition);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
    
    // Solve for tangent and bitangent using the inverse UV transformation
    float3 dp2perp = cross(dp2, worldNormal);
    float3 dp1perp = cross(worldNormal, dp1);
    
    float3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
    
    // Normalize to create orthonormal basis
    float invmax = rsqrt(max(dot(tangent, tangent), dot(bitangent, bitangent)));
    
    return float3x3(tangent * invmax, bitangent * invmax, worldNormal);
}

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;

    float3 texColor = diffuseTexture.Sample(samplerState, input.uv).rgb;
    float3 diffuseColor = texColor * materialDiffuse;

    // Normal map stores normals in tangent space with [0,1] encoding
    float3 normalMapSample = normalMapTexture.Sample(samplerState, input.uv).rgb;
    float3 tangentNormal = normalMapSample * 2.0f - 1.0f;
    
    float3 normalizedNormal = normalize(input.worldNormal);
    
    // Build TBN matrix on-the-fly using derivatives
    float3x3 TBN = ComputeTBN(input.worldPosition, normalizedNormal, input.uv);
    
    // Transform perturbed normal from tangent space to world space
    float3 worldNormal = normalize(mul(tangentNormal, TBN));

    // Pack material properties for G-Buffer
    float ambientStrength = saturate(dot(materialAmbient, float3(0.333f, 0.333f, 0.333f)));
    float specularStrength = saturate(dot(materialSpecular, float3(0.333f, 0.333f, 0.333f)));
    float specularPacked = saturate(specularPower / 256.0f);

    output.Albedo = float4(diffuseColor, ambientStrength);
    output.Normal = float4(worldNormal * 0.5f + 0.5f, specularStrength);
    output.Extra = float4(input.worldPosition, specularPacked);

    return output;
}
