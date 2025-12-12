cbuffer MatrixBuffer : register(b0)
{
    float4x4 worldMatrix;
    float4x4 viewProjMatrix;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float2 uv : UV;
};

VSOutput main(VertexInput input)
{
    VSOutput output;
    
    // Transform to world space only (NOT clip space yet)
    float4 worldPos = mul(float4(input.position, 1.0f), worldMatrix);
    output.worldPos = worldPos.xyz;
    
    // Transform normal to world space
    output.normal = normalize(mul(float4(input.normal, 0.0f), worldMatrix).xyz);
    
    output.uv = input.uv;
    return output;
}
