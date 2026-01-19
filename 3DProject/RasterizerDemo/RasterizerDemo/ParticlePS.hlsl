<<<<<<< Updated upstream
<<<<<<< Updated upstream
=======
=======
>>>>>>> Stashed changes
// ParticlePS.hlsl

struct PS_INPUT
{
    float4 clipPosition : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    // UV går (0..1). Flytta till centrum och gör en cirkelmask
    float2 p = input.uv * 2.0f - 1.0f; // (-1..1)
    float r2 = dot(p, p); // radius^2

    // Hård cutoff (billigt): disc
    // if (r2 > 1.0f) discard;

    // Mjuk kant (snyggare): 1 i mitten -> 0 vid kanten
    // "feather" styr hur mjuk kanten är (större = mjukare)
    const float feather = 0.15f;
    float alphaMask = 1.0f - smoothstep(1.0f - feather, 1.0f, r2);

    // Kombinera med partikelns alpha (som du nu fadar i compute shadern)
    float4 outColor = input.color;
    outColor.a *= alphaMask;

    // Om du vill att helt transparenta pixlar inte ska bidra alls:
    if (outColor.a <= 0.001f)
        discard;

    return outColor;
}
<<<<<<< Updated upstream
>>>>>>> Stashed changes
=======
>>>>>>> Stashed changes
