// Tessellation Domain Shader
// Interpolates tessellated vertices and applies Phong tessellation

cbuffer MatrixBuffer : register(b0)
{
    float4x4 worldMatrix;
    float4x4 viewProjMatrix;
};

struct HS_CONSTANT_OUTPUT
{
    float edges[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

struct HS_OUTPUT
{
    float3 worldPosition : WORLD_POSITION;
    float3 worldNormal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct DS_OUTPUT
{
    float4 clipPosition : SV_POSITION;
  float3 worldPosition : WORLD_POSITION;
    float3 worldNormal : NORMAL;
    float2 uv : TEXCOORD0;
};

#define NUM_CONTROL_POINTS 3

// Phong tessellation parameter (0 = linear interpolation, 1 = full Phong projection)
static const float PHONG_ALPHA = 0.75f;

// Project a position onto the plane defined by a point on the plane and its normal
float3 ProjectToPlane(float3 position, float3 planePoint, float3 planeNormal)
{
    // Calculate vector from plane point to the position we want to project
    float3 toPosition = position - planePoint;
    
    // Project onto normal and subtract to get the projected point
    float distanceToPlane = dot(toPosition, planeNormal);
    return position - distanceToPlane * planeNormal;
}

[domain("tri")]
DS_OUTPUT main(HS_CONSTANT_OUTPUT input, float3 barycentricCoords : SV_DomainLocation, const OutputPatch<HS_OUTPUT, NUM_CONTROL_POINTS> patch)
{
    DS_OUTPUT output;

    // Standard linear interpolation
    float3 linearPosition = patch[0].worldPosition * barycentricCoords.x + 
        patch[1].worldPosition * barycentricCoords.y + 
    patch[2].worldPosition * barycentricCoords.z;
    
    // Phong Tessellation: Project the linear position onto the three planes
    
    // Project onto plane 0
    float3 projection0 = ProjectToPlane(linearPosition, patch[0].worldPosition, patch[0].worldNormal);
    
    // Project onto plane 1
    float3 projection1 = ProjectToPlane(linearPosition, patch[1].worldPosition, patch[1].worldNormal);
    
    // Project onto plane 2
    float3 projection2 = ProjectToPlane(linearPosition, patch[2].worldPosition, patch[2].worldNormal);
    
    // Interpolate the projected positions using barycentric coordinates
    float3 phongPosition = projection0 * barycentricCoords.x + 
     projection1 * barycentricCoords.y + 
        projection2 * barycentricCoords.z;
    
    // Blend between linear interpolation and Phong projection
    output.worldPosition = lerp(linearPosition, phongPosition, PHONG_ALPHA);
    
    // Interpolate normal
    output.worldNormal = normalize(patch[0].worldNormal * barycentricCoords.x + 
        patch[1].worldNormal * barycentricCoords.y + 
        patch[2].worldNormal * barycentricCoords.z);
    
    // Interpolate UV
    output.uv = patch[0].uv * barycentricCoords.x + 
        patch[1].uv * barycentricCoords.y + 
        patch[2].uv * barycentricCoords.z;

    // Transform to clip space
    output.clipPosition = mul(float4(output.worldPosition, 1.0f), viewProjMatrix);

    return output;
}
