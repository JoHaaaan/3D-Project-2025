// Particle Update Compute Shader
// Updates particle simulation and handles respawning

cbuffer TimeBuffer : register(b0)
{
    float deltaTime;
    float3 emitterPosition;
    uint emitterEnabled;
    uint particleCount;
    float2 padding;
};

struct Particle
{
    float3 position;
    float lifetime;
    float3 velocity;
    float maxLifetime;
    float4 color;
};

RWStructuredBuffer<Particle> Particles : register(u0);

// Hash function for pseudo-random number generation
float Hash(float n)
{
    return frac(sin(n) * 43758.5453f);
}

// Generate random 3D vector
float3 Random3D(uint index, float time)
{
    float seed = (float)index * 12.9898f + time * 78.233f;
    return float3(Hash(seed + 1.0f), Hash(seed + 2.0f), Hash(seed + 3.0f));
}

// Respawn particle at emitter position with random velocity
void RespawnParticle(inout Particle particle, uint index, float seed)
{
    particle.position = emitterPosition;
    particle.lifetime = 0.0f;

    float3 randomValues = Random3D(index, seed);

    // Random velocity spread
    particle.velocity.x = (randomValues.x - 0.5f) * 4.0f;
    particle.velocity.z = (randomValues.z - 0.5f) * 4.0f;
    particle.velocity.y = 2.0f + randomValues.y * 2.0f;

    // Start with full alpha
    particle.color.a = 1.0f;
}

[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    if (index >= particleCount)
        return;

    Particle particle = Particles[index];

    // Handle inactive particles
    if (particle.lifetime < 0.0f)
    {
        if (emitterEnabled != 0)
        {
            RespawnParticle(particle, index, deltaTime);
        }

        Particles[index] = particle;
        return;
    }

    // Simulate active particles
    particle.position += particle.velocity * deltaTime;
    particle.velocity.y += -9.82f * deltaTime;
    particle.lifetime += deltaTime;

    // Fade based on lifetime
    float normalizedLifetime = saturate(particle.lifetime / max(particle.maxLifetime, 0.0001f));
    particle.color.a = 1.0f - normalizedLifetime;

    // Handle particle death
    if (particle.lifetime >= particle.maxLifetime)
    {
        if (emitterEnabled != 0)
        {
            RespawnParticle(particle, index, particle.maxLifetime + deltaTime);
        }
        else
        {
            // Mark as inactive
            particle.lifetime = -1.0f;
            particle.velocity = float3(0.0f, 0.0f, 0.0f);
            particle.color.a = 0.0f;
        }
    }

    Particles[index] = particle;
}

