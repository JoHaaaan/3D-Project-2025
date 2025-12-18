cbuffer MatrixBuffer : register(b0)
{
    float4x4 worldMatrix;
    float4x4 viewProjMatrix;
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

// Phong tessellation parameter (0 = linear interpolation, 1 = full Phong projection)
static const float PhongAlpha = 0.75f;

// Project a position onto the plane defined by a point on the plane and its normal
float3 ProjectToPlane(float3 pos, float3 planePoint, float3 planeNormal)
{
 // Calculate vector from plane point to the position we want to project
    float3 vec = pos - planePoint;
    
    // Project onto normal and subtract to get the projected point
    // This moves the position onto the plane
    float distance = dot(vec, planeNormal);
    return pos - distance * planeNormal;
}

[domain("tri")]
DomainShaderOutput main(HS_CONSTANT_DATA_OUTPUT input, float3 barycentric : SV_DomainLocation, const OutputPatch<HullShaderOutput, NUM_CONTROL_POINTS> patch)
{
    DomainShaderOutput output;

    // Standard linear interpolation (what we had before)
    float3 linearPos = patch[0].worldPos * barycentric.x + 
   patch[1].worldPos * barycentric.y + 
  patch[2].worldPos * barycentric.z;
    
    // Phong Tessellation: Project the linear position onto the three planes
    // defined by each vertex position and normal
    
    // Project onto plane 0 (defined by vertex 0 and its normal)
float3 proj0 = ProjectToPlane(linearPos, patch[0].worldPos, patch[0].normal);
    
    // Project onto plane 1 (defined by vertex 1 and its normal)
    float3 proj1 = ProjectToPlane(linearPos, patch[1].worldPos, patch[1].normal);
    
    // Project onto plane 2 (defined by vertex 2 and its normal)
    float3 proj2 = ProjectToPlane(linearPos, patch[2].worldPos, patch[2].normal);
    
    // Interpolate the projected positions using barycentric coordinates
    float3 phongPos = proj0 * barycentric.x + 
     proj1 * barycentric.y + 
        proj2 * barycentric.z;
    
    // Blend between linear interpolation and Phong projection
    // PhongAlpha = 0: pure linear (flat triangles)
// PhongAlpha = 1: full Phong projection (smoothest)
    output.worldPos = lerp(linearPos, phongPos, PhongAlpha);
    
    // Interpolate normal (same as before)
    output.normal = normalize(patch[0].normal * barycentric.x + 
      patch[1].normal * barycentric.y + 
  patch[2].normal * barycentric.z);
    
    // Interpolate UV (same as before)
    output.uv = patch[0].uv * barycentric.x + 
      patch[1].uv * barycentric.y + 
     patch[2].uv * barycentric.z;

    // Transform to clip space
    output.position = mul(float4(output.worldPos, 1), viewProjMatrix);

    return output;
}
