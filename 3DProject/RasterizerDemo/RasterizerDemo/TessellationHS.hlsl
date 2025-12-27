// Camera buffer for LOD calculation
cbuffer CameraBuffer : register(b3)
{
    float3 cameraPosition;
    float padding_Camera;
};

// LOD Settings - adjusted for better visual feedback
static const float MIN_TESS_DISTANCE = 1.0f;   // Close distance (max tessellation)
static const float MAX_TESS_DISTANCE = 10.0f;  // Far distance (min tessellation) - much longer range
static const float MAX_TESS_FACTOR = 4.0f;// Maximum tessellation - very low for clear visibility
static const float MIN_TESS_FACTOR = 1.0f;  // Minimum tessellation

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

// Calculate tessellation factor based on distance from camera
float CalculateTessellationFactor(float3 worldPos)
{
    // Calculate distance from camera to vertex
    float distance = length(cameraPosition - worldPos);
    
    // Clamp distance to our range
    distance = clamp(distance, MIN_TESS_DISTANCE, MAX_TESS_DISTANCE);
  
    // Linear interpolation from max to min tessellation
    // Close = MAX_TESS_FACTOR, Far = MIN_TESS_FACTOR
    float t = (distance - MIN_TESS_DISTANCE) / (MAX_TESS_DISTANCE - MIN_TESS_DISTANCE);
    float tessFactor = lerp(MAX_TESS_FACTOR, MIN_TESS_FACTOR, t);
    
    return tessFactor;
}

PatchConstantOutput PatchConstantFunc(InputPatch<HSInput, NUM_CONTROL_POINTS> patch, uint patchID : SV_PrimitiveID)
{
    PatchConstantOutput output;
    
    // Calculate tessellation factor for each edge based on the midpoint
    // This gives smoother transitions between tessellation levels
    
    // Edge 0: between vertex 0 and 1
    float3 edge0Midpoint = (patch[0].worldPos + patch[1].worldPos) * 0.5f;
    output.edges[0] = CalculateTessellationFactor(edge0Midpoint);
    
    // Edge 1: between vertex 1 and 2
    float3 edge1Midpoint = (patch[1].worldPos + patch[2].worldPos) * 0.5f;
    output.edges[1] = CalculateTessellationFactor(edge1Midpoint);
    
    // Edge 2: between vertex 2 and 0
    float3 edge2Midpoint = (patch[2].worldPos + patch[0].worldPos) * 0.5f;
    output.edges[2] = CalculateTessellationFactor(edge2Midpoint);
    
    // Inside tessellation: use average of the triangle's center
    float3 triangleCenter = (patch[0].worldPos + patch[1].worldPos + patch[2].worldPos) / 3.0f;
    output.inside = CalculateTessellationFactor(triangleCenter);
    
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
