cbuffer LightBuffer : register(b1)
{
    float3 lightPosition;
    float lightIntensity;
    float3 lightColor;
    float padding_Light;
};

cbuffer MaterialBuffer : register(b2)
{
    float3 materialAmbient;
    float padding1;

    float3 materialDiffuse;
    float padding2;

    float3 materialSpecular;
    float specularPower;
};

cbuffer CameraBuffer : register(b3)
{
    float3 cameraPosition;
    float padding_Camera;
};

// G-buffer
Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
Texture2D gWorldPos : register(t2);

// Output-bild
RWTexture2D<float4> outColor : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 pixel = DTid.xy;

    // Läs G-buffer
    float3 albedo = gAlbedo.Load(int3(pixel, 0)).rgb;
    float3 nPacked = gNormal.Load(int3(pixel, 0)).rgb;
    float3 worldPos = gWorldPos.Load(int3(pixel, 0)).xyz;

    // Packad normal [0,1] -> [-1,1]
    float3 normal = normalize(nPacked * 2.0f - 1.0f);

    // Samma ljuslogik som du hade i pixelshadern
    float3 lightDir = normalize(lightPosition - worldPos);
    float3 viewDir = normalize(cameraPosition - worldPos);
    float3 reflectDir = reflect(-lightDir, normal);

    float3 ambient = lightIntensity * lightColor * materialAmbient;

    float diffuseFactor = max(dot(normal, lightDir), 0.0);
    float3 diffuse = diffuseFactor * lightIntensity * lightColor * materialDiffuse;

    float specAngle = max(dot(viewDir, reflectDir), 0.0);
    float specularFactor = pow(specAngle, specularPower);
    float3 specular = specularFactor * lightIntensity * lightColor * materialSpecular;

    float3 lighting = ambient + diffuse + specular;

    outColor[pixel] = float4(lighting * albedo, 1.0f);
}
