// Normal Map Pixel Shader
// Uses partial derivatives to compute TBN matrix for normal mapping

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

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;

    // Sample diffuse texture
    float3 texColor = diffuseTexture.Sample(samplerState, input.uv).rgb;
    float3 diffuseColor = texColor * materialDiffuse;

    // Sample normal map (stored in tangent space, 0-1 range)
    float3 normalMapSample = normalMapTexture.Sample(samplerState, input.uv).rgb;
    
    // Convert from [0,1] to [-1,1] range
    float3 tangentNormal = normalMapSample * 2.0f - 1.0f;
    
    // Normalize the vertex normal
    float3 normalizedNormal = normalize(input.worldNormal);
    
    // Compute TBN matrix using partial derivatives
    float3x3 TBN = ComputeTBN(input.worldPosition, normalizedNormal, input.uv);
    
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
