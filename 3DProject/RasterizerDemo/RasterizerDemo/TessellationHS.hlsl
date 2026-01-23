// TESSELLATION HULL SHADER
// Calculates adaptive tessellation factors based on camera distance (LOD system)

cbuffer CameraBuffer : register(b3)
{
    float3 cameraPosition;
    float padding_Camera;
};

// Adaptive tessellation LOD settings
static const float MIN_TESS_DISTANCE = 1.0f;
static const float MAX_TESS_DISTANCE = 10.0f;
static const float MAX_TESS_FACTOR = 4.0f;
static const float MIN_TESS_FACTOR = 1.0f;

struct HS_INPUT
{
    float3 worldPosition : WORLD_POSITION;
    float3 worldNormal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct HS_OUTPUT
{
    float3 worldPosition : WORLD_POSITION;
    float3 worldNormal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct HS_CONSTANT_OUTPUT
{
    float edges[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

#define NUM_CONTROL_POINTS 3

// Dynamic LOD: more detail close to camera, less detail far away
float CalculateTessellationFactor(float3 worldPosition)
{
    float distance = length(cameraPosition - worldPosition);
    
    distance = clamp(distance, MIN_TESS_DISTANCE, MAX_TESS_DISTANCE);
  
    // Linear interpolation: close = high tessellation, far = low tessellation
    float t = (distance - MIN_TESS_DISTANCE) / (MAX_TESS_DISTANCE - MIN_TESS_DISTANCE);
    float tessellationFactor = lerp(MAX_TESS_FACTOR, MIN_TESS_FACTOR, t);
    
    return tessellationFactor;
}

// Patch constant function: runs once per triangle, determines subdivision level
HS_CONSTANT_OUTPUT PatchConstantFunc(InputPatch<HS_INPUT, NUM_CONTROL_POINTS> patch, uint patchID : SV_PrimitiveID)
{
    HS_CONSTANT_OUTPUT output;
    
    // Calculate tessellation for each edge based on edge midpoint distance
    float3 edge0Midpoint = (patch[0].worldPosition + patch[1].worldPosition) * 0.5f;
    output.edges[0] = CalculateTessellationFactor(edge0Midpoint);
  
    float3 edge1Midpoint = (patch[1].worldPosition + patch[2].worldPosition) * 0.5f;
    output.edges[1] = CalculateTessellationFactor(edge1Midpoint);
    
    float3 edge2Midpoint = (patch[2].worldPosition + patch[0].worldPosition) * 0.5f;
    output.edges[2] = CalculateTessellationFactor(edge2Midpoint);
    
    // Interior tessellation based on triangle center
    float3 triangleCenter = (patch[0].worldPosition + patch[1].worldPosition + patch[2].worldPosition) / 3.0f;
    output.inside = CalculateTessellationFactor(triangleCenter);
    
    return output;
}

// Hull shader configuration and control point passthrough
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(NUM_CONTROL_POINTS)]
[patchconstantfunc("PatchConstantFunc")]
HS_OUTPUT main(InputPatch<HS_INPUT, NUM_CONTROL_POINTS> patch, uint i : SV_OutputControlPointID)
{
    HS_OUTPUT output;
    output.worldPosition = patch[i].worldPosition;
    output.worldNormal = patch[i].worldNormal;
    output.uv = patch[i].uv;
    return output;
}
