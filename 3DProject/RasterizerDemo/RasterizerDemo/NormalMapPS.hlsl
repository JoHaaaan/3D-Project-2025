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
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

Texture2D diffuseTexture : register(t0);
Texture2D normalMapTexture : register(t1);
SamplerState samplerState : register(s0);

struct PS_OUTPUT
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Extra : SV_Target2;
};

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

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT o;

    // Sample diffuse texture
    float3 texColor = diffuseTexture.Sample(samplerState, input.uv).rgb;
    float3 diffuseColor = texColor * materialDiffuse;

    // Sample normal map (stored in tangent space, 0-1 range)
    float3 normalMapSample = normalMapTexture.Sample(samplerState, input.uv).rgb;
    
    // Convert from [0,1] to [-1,1] range
    float3 tangentNormal = normalMapSample * 2.0f - 1.0f;
    
    // Normalize the vertex normal
    float3 N = normalize(input.normal);
    
    // Compute TBN matrix using partial derivatives
    float3x3 TBN = ComputeTBN(input.worldPos, N, input.uv);
    
    // Transform normal from tangent space to world space
    float3 worldNormal = normalize(mul(tangentNormal, TBN));

    // Pack material properties same as regular pixel shader
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
