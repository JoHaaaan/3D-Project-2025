// LightingCS.hlsl

// ==== Konstanter ====

// Enklare punktljus
cbuffer LightBuffer : register(b1)
{
    float3 lightPosition;
    float lightIntensity;
    float3 lightColor;
    float padding_Light;
};

// Kameraposition för spekular osv.
cbuffer CameraBuffer : register(b2)
{
    float3 cameraPosition;
    float padding_Camera;
};

// Debug / toggles (1/0) styr hur vi visar belysningen
cbuffer LightingToggleBuffer : register(b4)
{
    int showAlbedoOnly; // 1 = visa bara albedo/diffuse
    int enableDiffuse; // 1 = lägg till diffuse-bidrag
    int enableSpecular; // 1 = lägg till specular-bidrag
    int paddingToggle; // oanvänd, bara för alignment
};

// ==== G-buffer-resurser ====
//
// Layout (måste matcha pixelshadern!):
//  t0: gAlbedo    = float4(diffuseColor.rgb, ambientStrength)
//  t1: gNormal    = float4(packedNormal.rgb, specularStrength)
//  t2: gWorldPos  = float4(worldPos.xyz, shininessPacked)
//
Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
Texture2D gWorldPos : register(t2);

// Output-bild (skrivs av compute-shadern)
RWTexture2D<float4> outColor : register(u0);

// En threadgroup på 16x16 trådar, matchar dispatch i C++
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 pixel = DTid.xy;

    // Skydd mot att köra utanför bildens kanter
    uint w, h;
    gAlbedo.GetDimensions(w, h);
    if (pixel.x >= w || pixel.y >= h)
        return;

    // ==== Läs G-buffer enligt nya layouten ====

    // RT0: diffuse.rgb, ambientStrength.a
    float4 albedoSample = gAlbedo.Load(int3(pixel, 0));
    float3 diffuseColor = albedoSample.rgb;
    float ambientStrength = albedoSample.a;

    // RT1: packad normal.rgb, specularStrength.a
    float4 normalSample = gNormal.Load(int3(pixel, 0));
    float3 nPacked = normalSample.rgb;
    float specularStrength = normalSample.a;

    // RT2: worldPos.xyz, shininess-packad.w
    float4 posSample = gWorldPos.Load(int3(pixel, 0));
    float3 worldPos = posSample.xyz;
    float specPacked = posSample.w;

    // Avpacka normal [0,1] -> [-1,1]
    float3 normal = normalize(nPacked * 2.0f - 1.0f);

    // Avpacka shininess ~ [1,256]
    float specularPower = max(specPacked * 256.0f, 1.0f);

    // Materialkomponenter per pixel (nu baserat på lagrad data)
    float3 materialDiffuse = diffuseColor;
    float3 materialAmbient = ambientStrength * diffuseColor;
    float3 materialSpecular = specularStrength * float3(1.0f, 1.0f, 1.0f);

    // ==== Debug-läge: visa bara albedo/diffuse ====
    if (showAlbedoOnly != 0)
    {
        outColor[pixel] = float4(diffuseColor, 1.0f);
        return;
    }

    // ==== Phong/Blinn-liknande belysning per pixel ====

    float3 lightDir = normalize(lightPosition - worldPos);
    float3 viewDir = normalize(cameraPosition - worldPos);
    float3 reflectDir = reflect(-lightDir, normal);

    // Ambient
    float3 ambient = lightIntensity * lightColor * materialAmbient;

    // Diffuse
    float diffuseFactor = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = diffuseFactor * lightIntensity * lightColor * materialDiffuse;

    // Specular
    float specAngle = max(dot(viewDir, reflectDir), 0.0f);
    float specularFactor = pow(specAngle, specularPower);
    float3 specular = specularFactor * lightIntensity * lightColor * materialSpecular;

    // Bygg upp slutlig belysning med toggles
    float3 lighting = ambient; // ambient alltid med (om du inte vill toggla den också)

    if (enableDiffuse != 0)
    {
        lighting += diffuse;
    }

    if (enableSpecular != 0)
    {
        lighting += specular;
    }

    // Skriv färdig pixel till output
    outColor[pixel] = float4(lighting, 1.0f);
}
