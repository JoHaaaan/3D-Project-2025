// ParticleUpdateCS.hlsl

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

RWStructuredBuffer<Particle> Particles : register(u0);

float Hash(float n)
{
    return frac(sin(n) * 43758.5453f);
}

float3 Rand3(uint index, float t)
{
    float s = (float) index * 12.9898f + t * 78.233f;
    return float3(Hash(s + 1.0f), Hash(s + 2.0f), Hash(s + 3.0f));
}

// NEW: gemensam respawn-funktion
void RespawnParticle(inout Particle p, uint index, float seed)
{
    p.position = emitterPosition;
    p.lifetime = 0.0f;

    float3 r = Rand3(index, seed);

    // random velocity i [min..max]
    p.velocity = lerp(velocityMin, velocityMax, r);

    // reset alpha
    p.color.a = 1.0f;
}

[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    if (index >= particleCount)
        return;

    Particle p = Particles[index];

    if (p.lifetime < 0.0f)
    {
        if (emitterEnabled != 0)
            RespawnParticle(p, index, deltaTime);

        Particles[index] = p;
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

    Particles[index] = p;
}
