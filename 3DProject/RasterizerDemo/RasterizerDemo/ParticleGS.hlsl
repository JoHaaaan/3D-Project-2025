// All camera data i EN buffer! (DETTA ÄR ÄNDRINGEN!)
cbuffer ParticleCameraBuffer : register(b0)
{
    float4x4 viewProjection;
    float3 cameraPosition;
    float padding;
};

// Input från vertex shader
struct GS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
};

// Output till pixel shader
struct GS_OUTPUT
{
    float4 clipPosition : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

// Skapa 6 vertices (2 trianglar = 1 quad) per punkt
[maxvertexcount(6)]
void main(point GS_INPUT input[1], inout TriangleStream<GS_OUTPUT> output)
{
    // Particle position i world space
    float3 particlePos = input[0].position;
    
    // Beräkna billboarding vektorer
    // Front vector: från kamera till partikel
    float3 frontVec = normalize(particlePos - cameraPosition);
    
    // Right vector: perpendicular till front och world up
    float3 worldUp = float3(0.0f, 1.0f, 0.0f);
    float3 rightVec = normalize(cross(worldUp, frontVec));
    
    // Up vector: perpendicular till front och right (actual up för quad)
    float3 upVec = normalize(cross(frontVec, rightVec));
    
    // Quad storlek (justera efter behov - större = mer synligt)
    float quadSize = 0.5f;
    rightVec *= quadSize;
    upVec *= quadSize;
    
    // Skapa quad corners i world space
    float3 topLeft = particlePos - rightVec + upVec;
    float3 topRight = particlePos + rightVec + upVec;
    float3 bottomLeft = particlePos - rightVec - upVec;
    float3 bottomRight = particlePos + rightVec - upVec;
    
    // Första triangeln (top-left, bottom-right, bottom-left)
    GS_OUTPUT v;
    
    // Top-left
    v.clipPosition = mul(float4(topLeft, 1.0f), viewProjection);
    v.color = input[0].color;
    v.uv = float2(0.0f, 0.0f);
    output.Append(v);
    
    // Bottom-right
    v.clipPosition = mul(float4(bottomRight, 1.0f), viewProjection);
    v.color = input[0].color;
    v.uv = float2(1.0f, 1.0f);
    output.Append(v);
    
    // Bottom-left
    v.clipPosition = mul(float4(bottomLeft, 1.0f), viewProjection);
    v.color = input[0].color;
    v.uv = float2(0.0f, 1.0f);
    output.Append(v);
    
    output.RestartStrip();
    
    // Andra triangeln (top-left, top-right, bottom-right)
    
    // Top-left
    v.clipPosition = mul(float4(topLeft, 1.0f), viewProjection);
    v.color = input[0].color;
    v.uv = float2(0.0f, 0.0f);
    output.Append(v);
    
    // Top-right
    v.clipPosition = mul(float4(topRight, 1.0f), viewProjection);
    v.color = input[0].color;
    v.uv = float2(1.0f, 0.0f);
    output.Append(v);
    
    // Bottom-right
    v.clipPosition = mul(float4(bottomRight, 1.0f), viewProjection);
    v.color = input[0].color;
    v.uv = float2(1.0f, 1.0f);
    output.Append(v);
}