// PARTICLE UPDATE COMPUTE SHADER
// GPU-based particle simulation using compute shaders
// Demonstrates: UAV write access, parallel processing, particle lifecycle management

cbuffer TimeBuffer : register(b0)
{
    float deltaTime;
    float3 emitterPosition;

    uint emitterEnabled;
    uint particleCount;
    float pad2;
    float pad3;
    float3 velocityMin;
    float pad4;
    float3 velocityMax;
    float pad5;
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

// Respawn particle with random velocity in a defined range
void RespawnParticle(inout Particle particle, uint index, float seed)
{
    particle.position = emitterPosition;

    float3 randomValues = Random3D(index, seed);

    // Randomized initial velocity
    particle.velocity = lerp(velocityMin, velocityMax, randomValues);

    // Random lifetime offset to avoid synchronized respawns
    particle.lifetime = randomValues.y * particle.maxLifetime;

    particle.color.a = 1.0f;
}

// Compute shader dispatch: 32 threads per group, processes all particles in parallel
[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    if (index >= particleCount)
        return;

    Particle particle = Particles[index];
    
    // Handle inactive/dead particles
    if (particle.lifetime < 0.0f)
    {
        if (emitterEnabled != 0)
            RespawnParticle(particle, index, (float) index + particle.maxLifetime);

        Particles[index] = particle;
        return;
    }

    // Physics simulation: Euler integration with gravity
    particle.position += particle.velocity * deltaTime;
    particle.velocity.y += -2.0f * deltaTime; // Gravity acceleration (-2 for snowlike effect)
    particle.lifetime += deltaTime;

    float normalizedLifetime = saturate(particle.lifetime / max(particle.maxLifetime, 0.0001f));
    particle.color.a = 1.0f - normalizedLifetime;

    if (particle.lifetime >= particle.maxLifetime)
    {
        if (emitterEnabled != 0)
            RespawnParticle(particle, index, (float) index + particle.maxLifetime + particle.lifetime);
        else
        {
            particle.lifetime = -1.0f;
            particle.velocity = float3(0, 0, 0);
            particle.color.a = 0.0f;
        }
    }

    // Write updated particle back to buffer
    Particles[index] = particle;
}
