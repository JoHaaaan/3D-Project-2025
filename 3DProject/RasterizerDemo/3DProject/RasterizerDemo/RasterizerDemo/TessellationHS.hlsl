#include "TessellationCommon.hlsl"

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

HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(InputPatch<VertexShaderOutput, NUM_CONTROL_POINTS> ip)
{
    HS_CONSTANT_DATA_OUTPUT output;
    output.EdgeTessFactor[0] = output.EdgeTessFactor[1] = output.EdgeTessFactor[2] = 32;
    output.InsideTessFactor = 32;
    return output;
}

struct HullShaderOutput
{
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float2 uv : UV;
};

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("CalcHSPatchConstants")]
HullShaderOutput HSMain(
    InputPatch<VertexShaderOutput, NUM_CONTROL_POINTS> ip,
    uint i : SV_OutputControlPointID)
{
    HullShaderOutput output;

    output.worldPos = ip[i].worldPos;
    output.normal = ip[i].normal;
    output.uv = ip[i].uv;

    return output;
}

cbuffer Camera : register(b0)
{
    float4x4 vp;
};

struct DomainShaderOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float2 uv : UV;
};

[domain("tri")]
DomainShaderOutput DSMain(HS_CONSTANT_DATA_OUTPUT input, float3 barycentric : SV_DomainLocation, const OutputPatch<HullShaderOutput, NUM_CONTROL_POINTS> patch)
{
    DomainShaderOutput output;

    output.worldPos = patch[0].worldPos * barycentric.x + patch[1].worldPos * barycentric.y + patch[2].worldPos * barycentric.z;
    output.normal = normalize(patch[0].normal * barycentric.x + patch[1].normal * barycentric.y + patch[2].normal * barycentric.z);
    output.uv = patch[0].uv * barycentric.x + patch[1].uv * barycentric.y + patch[2].uv * barycentric.z;

    output.position = mul(float4(output.worldPos, 1), vp);

    return output;
}
