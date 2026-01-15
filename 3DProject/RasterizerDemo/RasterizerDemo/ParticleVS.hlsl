// Particle struct - MÅSTE matcha C++ Particle struct!
struct Particle
{
    float3 position; // 12 bytes
    float lifetime; // 4 bytes
    
    float3 velocity; // 12 bytes
    float maxLifetime; // 4 bytes
    
    float4 color; // 16 bytes
};

// Particle buffer bunden som structured buffer (vertex pulling!)
StructuredBuffer<Particle> Particles : register(t0);

// Output till geometry shader
struct VS_OUTPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
};

VS_OUTPUT main(uint vertexID : SV_VertexID)
{
    VS_OUTPUT output;
    
    // Vertex pulling - hämta particle data manuellt med vertexID
    Particle p = Particles[vertexID];
    
    // Skicka position och färg till geometry shader
    output.position = p.position;
    output.color = p.color;
    
    return output;
}