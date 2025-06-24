cbuffer MatrixBuffer : register(b0)
{
    float4x4 worldMatrix;
    float4x4 viewProjMatrix;
};

struct VertexShaderInput
{
    float3 position : POSITION;   // Input position in local space
    float3 normal   : NORMAL;     // Input normal in local space
    float2 uv       : TEXCOORD;   // Input Texture coordinates
};

struct VertexShaderOutput
{
    float4 clipPosition : SV_POSITION;    // Output position in clip space for rasterization
    float3 worldPos     : WORLD_POSITION; // Output world-space position for lighting
    float3 worldNormal  : NORMAL0;        // Output Normal in world space for lighting
    float2 uv           : TEXCOORD0;      // Output texture coordinates
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // Transform the position to world space
    float4 worldPosition = mul(float4(input.position, 1.0f), worldMatrix);
    output.worldPos = worldPosition.xyz;

    // Transform the position to clip space (combined view + projection)
    output.clipPosition = mul(worldPosition, viewProjMatrix);

    // Transform the normal to world space and normalize
    output.worldNormal = normalize(mul(float4(input.normal, 0.0f), worldMatrix).xyz);

    // Pass through UV coordinates
    output.uv = input.uv;

    return output;
}
