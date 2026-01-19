// ParticleUpdateCS.hlsl

cbuffer TimeBuffer : register(b0)
{
    float deltaTime;
    float3 emitterPosition;

    uint emitterEnabled;
    uint particleCount;
    float2 pad;
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

// FIX 1: Samla all respawn-logik p� ett st�lle
void RespawnParticle(inout Particle p, uint index, float seed)
{
    p.position = emitterPosition;
    p.lifetime = 0.0f;

    float3 r = Rand3(index, seed);

    // Samma "shape" p� spridningen som du haft innan
    p.velocity.x = (r.x - 0.5f) * 4.0f;
    p.velocity.z = (r.z - 0.5f) * 4.0f;
    p.velocity.y = 2.0f + r.y * 2.0f;

    // Starta alltid med full alpha
    p.color.a = 1.0f;
}

[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    if (index >= particleCount)
        return;

    Particle p = Particles[index];

    // Inaktiv partikel (pool)
    if (p.lifetime < 0.0f)
    {
        if (emitterEnabled != 0)
        {
            // FIX 1: anropa helper ist�llet f�r duplicerad kod
            RespawnParticle(p, index, deltaTime);
        }

        Particles[index] = p;
        return;
    }

    // Simulera aktiv
    p.position += p.velocity * deltaTime;
    p.velocity.y += -9.82f * deltaTime;
    p.lifetime += deltaTime;

    // Fade (0..1) baserat p� livsl�ngd
    float t = saturate(p.lifetime / max(p.maxLifetime, 0.0001f));
    p.color.a = 1.0f - t;

    // D�d?
    if (p.lifetime >= p.maxLifetime)
    {
        if (emitterEnabled != 0)
        {
            // FIX 1: samma helper h�r ocks�
            RespawnParticle(p, index, p.maxLifetime + deltaTime);
        }
        else
        {
            // Emitter av: markera som inaktiv
            p.lifetime = -1.0f;
            p.velocity = float3(0, 0, 0);
            p.color.a = 0.0f;
        }
    }

    Particles[index] = p;
}

