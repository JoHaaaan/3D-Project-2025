// NORMAL MAP PIXEL SHADER
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
Texture2D normalMapTexture : register(t1);
SamplerState samplerState : register(s0);

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;

    float3 texColor = diffuseTexture.Sample(samplerState, input.uv).rgb;
    float3 diffuseColor = texColor * materialDiffuse;

    // Normal map stores normals in tangent space with [0,1] encoding
    float3 normalMapSample = normalMapTexture.Sample(samplerState, input.uv).rgb;
    float3 tangentNormal = normalMapSample * 2.0f - 1.0f;
    
    // Normalize TBN basis vectors (interpolation may have denormalized them)
    float3 T = normalize(input.worldTangent);
    float3 B = normalize(input.worldBitangent);
    float3 N = normalize(input.worldNormal);
 
    // Build TBN matrix from pre-computed vertex shader values
    float3x3 TBN = float3x3(T, B, N);
    
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
