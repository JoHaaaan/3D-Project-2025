// Tessellation Hull Shader
// Calculates adaptive tessellation based on distance from camera

cbuffer CameraBuffer : register(b3)
{
    float3 cameraPosition;
    float padding_Camera;
};

// LOD Settings
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

// Calculate tessellation factor based on distance from camera
float CalculateTessellationFactor(float3 worldPosition)
{
    // Calculate distance from camera to vertex
    float distance = length(cameraPosition - worldPosition);
    
    // Clamp distance to our range
    distance = clamp(distance, MIN_TESS_DISTANCE, MAX_TESS_DISTANCE);
  
    // Linear interpolation from max to min tessellation
    float t = (distance - MIN_TESS_DISTANCE) / (MAX_TESS_DISTANCE - MIN_TESS_DISTANCE);
    float tessellationFactor = lerp(MAX_TESS_FACTOR, MIN_TESS_FACTOR, t);
    
  return tessellationFactor;
}

HS_CONSTANT_OUTPUT PatchConstantFunc(InputPatch<HS_INPUT, NUM_CONTROL_POINTS> patch, uint patchID : SV_PrimitiveID)
{
    HS_CONSTANT_OUTPUT output;
    
    // Calculate tessellation factor for each edge based on the midpoint
    
    // Edge 0: between vertex 0 and 1
    float3 edge0Midpoint = (patch[0].worldPosition + patch[1].worldPosition) * 0.5f;
    output.edges[0] = CalculateTessellationFactor(edge0Midpoint);
  
    // Edge 1: between vertex 1 and 2
    float3 edge1Midpoint = (patch[1].worldPosition + patch[2].worldPosition) * 0.5f;
    output.edges[1] = CalculateTessellationFactor(edge1Midpoint);
    
    // Edge 2: between vertex 2 and 0
    float3 edge2Midpoint = (patch[2].worldPosition + patch[0].worldPosition) * 0.5f;
    output.edges[2] = CalculateTessellationFactor(edge2Midpoint);
    
    // Inside tessellation: use average of the triangle's center
    float3 triangleCenter = (patch[0].worldPosition + patch[1].worldPosition + patch[2].worldPosition) / 3.0f;
    output.inside = CalculateTessellationFactor(triangleCenter);
    
    return output;
}

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
