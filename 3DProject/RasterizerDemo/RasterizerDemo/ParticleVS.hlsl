// Particle Vertex Shader
// Reads particle data from structured buffer

struct Particle
{
    float3 position;
    float lifetime;
    float3 velocity;
    float maxLifetime;
    float4 color;
};

struct VS_OUTPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float lifetime : TEXCOORD0;
};

StructuredBuffer<Particle> Particles : register(t0);

VS_OUTPUT main(uint vertexID : SV_VertexID)
{
    VS_OUTPUT output;

    Particle particle = Particles[vertexID];

    output.position = particle.position;
    output.color = particle.color;
    output.lifetime = particle.lifetime;

    return output;
}
