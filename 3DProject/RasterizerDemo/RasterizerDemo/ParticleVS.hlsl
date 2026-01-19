// ParticleVS.hlsl

struct Particle
{
    float3 position;
    float lifetime;

    float3 velocity;
    float maxLifetime;

    float4 color;
};

StructuredBuffer<Particle> Particles : register(t0);

struct VS_OUTPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float lifetime : TEXCOORD1; // skicka vidare till GS
};

VS_OUTPUT main(uint vertexID : SV_VertexID)
{
    VS_OUTPUT o;

    Particle p = Particles[vertexID];

    o.position = p.position;
    o.color = p.color;
    o.lifetime = p.lifetime;

    return o;
}
