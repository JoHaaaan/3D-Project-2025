// ========================================
// TESSELLATION DOMAIN SHADER
// ========================================
// Part 3 of 4: Tessellation Pipeline (VS -> HS -> Tessellator -> DS -> PS)
// Evaluates tessellated vertices using Phong tessellation for smooth surfaces

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

// Phong tessellation smoothing (0 = linear, 1 = full smooth projection)
static const float PHONG_ALPHA = 0.75f;

// Projects a point onto a tangent plane to create curved surface
float3 ProjectToPlane(float3 position, float3 planePoint, float3 planeNormal)
{
    float3 toPosition = position - planePoint;
    float distanceToPlane = dot(toPosition, planeNormal);
    return position - distanceToPlane * planeNormal;
}

[domain("tri")]
DS_OUTPUT main(HS_CONSTANT_OUTPUT input, float3 barycentricCoords : SV_DomainLocation, const OutputPatch<HS_OUTPUT, NUM_CONTROL_POINTS> patch)
{
    DS_OUTPUT output;

    // Linear interpolation of control point positions
    float3 linearPosition = patch[0].worldPosition * barycentricCoords.x + 
        patch[1].worldPosition * barycentricCoords.y + 
        patch[2].worldPosition * barycentricCoords.z;
    
    // Phong Tessellation: smooth the surface by projecting onto control point tangent planes
    // This creates a curved approximation instead of flat triangles
    
    float3 projection0 = ProjectToPlane(linearPosition, patch[0].worldPosition, patch[0].worldNormal);
    float3 projection1 = ProjectToPlane(linearPosition, patch[1].worldPosition, patch[1].worldNormal);
    float3 projection2 = ProjectToPlane(linearPosition, patch[2].worldPosition, patch[2].worldNormal);
    
    // Barycentric-weighted average of projections
    float3 phongPosition = projection0 * barycentricCoords.x + 
        projection1 * barycentricCoords.y + 
        projection2 * barycentricCoords.z;
    
    // Blend between flat and smooth (PHONG_ALPHA controls smoothing strength)
    output.worldPosition = lerp(linearPosition, phongPosition, PHONG_ALPHA);
    
    // Interpolate normal for lighting
    output.worldNormal = normalize(patch[0].worldNormal * barycentricCoords.x + 
        patch[1].worldNormal * barycentricCoords.y + 
        patch[2].worldNormal * barycentricCoords.z);
    
    // Interpolate texture coordinates
    output.uv = patch[0].uv * barycentricCoords.x + 
        patch[1].uv * barycentricCoords.y + 
        patch[2].uv * barycentricCoords.z;

    // Final projection to clip space (this is why VS doesn't do it)
    output.clipPosition = mul(float4(output.worldPosition, 1.0f), viewProjMatrix);

    return output;
}
