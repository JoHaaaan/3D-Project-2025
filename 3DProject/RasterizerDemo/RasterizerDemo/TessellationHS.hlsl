struct HSInput
{
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float2 uv : UV;
};

struct HSOutput
{
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float2 uv : UV;
};

struct PatchConstantOutput
{
    float edges[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

#define NUM_CONTROL_POINTS 3

PatchConstantOutput PatchConstantFunc(InputPatch<HSInput, NUM_CONTROL_POINTS> patch, uint patchID : SV_PrimitiveID)
{
    PatchConstantOutput output;
    
    // Set tessellation factors (adjustable)
    output.edges[0] = output.edges[1] = output.edges[2] = 16.0f;
    output.inside = 16.0f;
    
    return output;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(NUM_CONTROL_POINTS)]
[patchconstantfunc("PatchConstantFunc")]
HSOutput main(InputPatch<HSInput, NUM_CONTROL_POINTS> patch, uint i : SV_OutputControlPointID)
{
    HSOutput output;
    output.worldPos = patch[i].worldPos;
    output.normal = patch[i].normal;
    output.uv = patch[i].uv;
    return output;
}
