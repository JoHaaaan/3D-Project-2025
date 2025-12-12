cbuffer Camera : register(b0)
{
    float4x4 vp;
};

struct VertexShaderOutput
{
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float2 uv : UV;
};

struct HS_CONSTANT_DATA_OUTPUT
{
    float EdgeTessFactor[3] : SV_TessFactor;
    float InsideTessFactor : SV_InsideTessFactor;
};

#define NUM_CONTROL_POINTS 3

struct HullShaderOutput
{
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float2 uv : UV;
};

struct DomainShaderOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float2 uv : UV;
};

[domain("tri")]
DomainShaderOutput main(HS_CONSTANT_DATA_OUTPUT input, float3 barycentric : SV_DomainLocation, const OutputPatch<HullShaderOutput, NUM_CONTROL_POINTS> ip)
{
    DomainShaderOutput output;
    
    output.worldPos = ip[0].worldPos * barycentric.x + 
                      ip[1].worldPos * barycentric.y + 
                      ip[2].worldPos * barycentric.z;
    
    output.normal = normalize(ip[0].normal * barycentric.x + 
                              ip[1].normal * barycentric.y + 
                              ip[2].normal * barycentric.z);
    
    output.uv = ip[0].uv * barycentric.x + 
                ip[1].uv * barycentric.y + 
                ip[2].uv * barycentric.z;
    
    output.position = mul(float4(output.worldPos, 1.0f), vp);
    
    return output;
}
