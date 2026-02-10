// PARALLAX OCCLUSION MAPPING VERTEX SHADER
// Computes TBN matrix on-the-fly using Gram-Schmidt orthogonalization
// Key technique: Derives tangent from normal, then computes bitangent via cross product

cbuffer MatrixBuffer : register(b0)
{
float4x4 worldMatrix;
    float4x4 viewProjMatrix;
};

struct VS_INPUT
{
  float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 clipPosition : SV_POSITION;
    float3 worldPosition : WORLD_POSITION;
    float3 worldNormal : NORMAL;
    float3 worldTangent : TANGENT;
    float3 worldBitangent : BITANGENT;
    float2 uv : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    // Transform position to world and clip space
    float4 worldPosition = mul(float4(input.position, 1.0f), worldMatrix);
    output.worldPosition = worldPosition.xyz;
    output.clipPosition = mul(worldPosition, viewProjMatrix);

    // Transform normal to world space
    float3 worldNormal = normalize(mul(float4(input.normal, 0.0f), worldMatrix).xyz);
    output.worldNormal = worldNormal;

    // Compute tangent on-the-fly using Gram-Schmidt orthogonalization
    // Choose a helper vector that's not parallel to the normal
    float3 helper = abs(worldNormal.x) > 0.9f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    
    // Create an initial tangent perpendicular to the normal
    float3 tangent = cross(worldNormal, helper);
    
    // Gram-Schmidt: Make tangent orthogonal to normal
    tangent = normalize(tangent - dot(tangent, worldNormal) * worldNormal);
    output.worldTangent = tangent;

    // Compute bitangent as cross product of normal and tangent
    float3 bitangent = cross(worldNormal, tangent);
    output.worldBitangent = normalize(bitangent);

    // Pass through UV coordinates
    output.uv = input.uv;

    return output;
}
