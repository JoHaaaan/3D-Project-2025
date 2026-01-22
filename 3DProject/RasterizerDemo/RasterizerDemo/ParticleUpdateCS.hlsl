// =====================================================================
// PARTICLE UPDATE COMPUTE SHADER
// =====================================================================
// GPU-based particle simulation using compute shaders
// Demonstrates: UAV write access, parallel processing, particle lifecycle management

cbuffer TimeBuffer : register(b0)
{
    float deltaTime;
    float3 emitterPosition;

    uint emitterEnabled;
    uint particleCount;

    float3 velocityMin; // NEW
    float pad2; // align

    float3 velocityMax; // NEW
    float pad3; // align
};

struct Particle
{
    float3 position;
    float lifetime;
    float3 velocity;
    float maxLifetime;
    float4 color;
};

// Read-write access to particle buffer (UAV = Unordered Access View)
RWStructuredBuffer<Particle> Particles : register(u0);

// Pseudo-random number generation for particle spawning
float Hash(float n)
{
    return frac(sin(n) * 43758.5453f);
}

float3 Random3D(uint index, float time)
{
    float seed = (float)index * 12.9898f + time * 78.233f;
    return float3(Hash(seed + 1.0f), Hash(seed + 2.0f), Hash(seed + 3.0f));
}

// NEW: gemensam respawn-funktion
void RespawnParticle(inout Particle p, uint index, float seed)
{
    particle.position = emitterPosition;
    particle.lifetime = 0.0f;

    float3 randomValues = Random3D(index, seed);

    // random velocity i [min..max]
    p.velocity = lerp(velocityMin, velocityMax, r);

    // reset alpha
    p.color.a = 1.0f;
}

// Compute shader dispatch: 32 threads per group, processes all particles in parallel
[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    if (index >= particleCount)
        return;

    Particle particle = Particles[index];

    if (p.lifetime < 0.0f)
    {
        if (emitterEnabled != 0)
            RespawnParticle(p, index, deltaTime);

        Particles[index] = particle;
        return;
    }

    // simulate
    p.position += p.velocity * deltaTime;
    p.velocity.y += -2.82f * deltaTime;
    p.lifetime += deltaTime;

    float t = saturate(p.lifetime / max(p.maxLifetime, 0.0001f));
    p.color.a = 1.0f - t;

    if (p.lifetime >= p.maxLifetime)
    {
        if (emitterEnabled != 0)
            RespawnParticle(p, index, p.maxLifetime + deltaTime);
        else
        {
            p.lifetime = -1.0f;
            p.velocity = float3(0, 0, 0);
            p.color.a = 0.0f;
        }
    }

    // Write updated particle back to buffer
    Particles[index] = particle;
}
