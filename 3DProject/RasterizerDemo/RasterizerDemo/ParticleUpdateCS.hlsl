// Constant buffer för delta time
cbuffer TimeBuffer : register(b0)
{
    float deltaTime;
    float3 padding;
};

// Particle struct - MÅSTE matcha C++ Particle struct!
struct Particle
{
    float3 position; // 12 bytes
    float lifetime; // 4 bytes
    
    float3 velocity; // 12 bytes
    float maxLifetime; // 4 bytes
    
    float4 color; // 16 bytes
};

// Read/Write structured buffer (UAV)
RWStructuredBuffer<Particle> Particles : register(u0);

// 32 threads per grupp (matchar dispatch i C++)
[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // Hämta partikeln
    Particle p = Particles[DTid.x];
    
    // Uppdatera position baserat på velocity
    p.position += p.velocity * deltaTime;
    
    // Applicera gravity (neråt kraft)
    float gravity = -9.82f;
    p.velocity.y += gravity * deltaTime;
    
    // Uppdatera lifetime
    p.lifetime += deltaTime;
    
    // Om partikeln har "dött", återställ den
    if (p.lifetime >= p.maxLifetime)
    {
        // Återställ till emitter position (anta 0,0,0 för nu)
        // OBS: Du kan skicka emitter position via constant buffer om du vill!
        p.position = float3(0.0f, 0.0f, 0.0f);
        
        // Återställ lifetime
        p.lifetime = 0.0f;
        
        // Ge ny slumpmässig velocity (enkel version)
        // För bättre randomness, använd en seed i constant buffer
        float seed = DTid.x * deltaTime;
        p.velocity.x = (frac(sin(seed * 12.9898f) * 43758.5453f) - 0.5f) * 4.0f;
        p.velocity.y = -5.0f + frac(sin(seed * 78.233f) * 43758.5453f) * 4.0f;
        p.velocity.z = (frac(sin(seed * 45.164f) * 43758.5453f) - 0.5f) * 4.0f;
    }
    
    // Skriv tillbaka uppdaterad partikel
    Particles[DTid.x] = p;
}