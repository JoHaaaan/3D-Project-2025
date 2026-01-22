// ========================================
// TESSELLATION VERTEX SHADER
// ========================================
// Part 1 of 4: Tessellation Pipeline (VS -> HS -> Tessellator -> DS -> PS)
// Prepares mesh data in world space for tessellation stages

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
    float3 worldPosition : WORLD_POSITION;
    float3 worldNormal : NORMAL;
    float2 uv : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // Transform to world space ONLY (clip space projection happens in domain shader)
    float4 worldPosition = mul(float4(input.position, 1.0f), worldMatrix);
    output.worldPosition = worldPosition.xyz;
    
    // Transform normal to world space
    output.worldNormal = normalize(mul(float4(input.normal, 0.0f), worldMatrix).xyz);
    
    // Pass through UV coordinates
    output.uv = input.uv;

    return output;
}
