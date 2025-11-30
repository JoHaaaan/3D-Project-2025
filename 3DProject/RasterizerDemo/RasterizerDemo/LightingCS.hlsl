cbuffer LightBuffer : register(b1)
{
    float3 lightPosition;
    float lightIntensity;
    float3 lightColor;
    float padding_Light;
};

cbuffer CameraBuffer : register(b2)
{
    float3 cameraPosition;
    float padding_Camera;
};

cbuffer LightingToggleBuffer : register(b4)
{
    int showAlbedoOnly; // 1 = show only albedo
    int enableDiffuse; // 1 = enable diffuse
    int enableSpecular; // 1 = enable specular
    int paddingToggle; // unused, padding
};

// G-buffer
Texture2D gAlbedo : register(t0); // rgb = albedo, a = specPower/256
Texture2D gNormal : register(t1); // packed normal
Texture2D gWorldPos : register(t2); // world position

// Output image
RWTexture2D<float4> outColor : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 pixel = DTid.xy;

    uint w, h;
    gAlbedo.GetDimensions(w, h);
    if (pixel.x >= w || pixel.y >= h)
        return;

    // Read G-buffer
    float4 albedoSample = gAlbedo.Load(int3(pixel, 0));
    float3 albedo = albedoSample.rgb;
    float specPacked = albedoSample.a;

    float3 nPacked = gNormal.Load(int3(pixel, 0)).rgb;
    float3 worldPos = gWorldPos.Load(int3(pixel, 0)).xyz;

    // Unpack normal from [0,1] to [-1,1]
    float3 normal = normalize(nPacked * 2.0f - 1.0f);

    // Restore specular power (stored as specPower / 256)
    float specularPower = max(specPacked * 256.0f, 1.0f);

    // Build material per pixel
    float3 materialDiffuse = albedo;
    float3 materialAmbient = 0.2f * albedo;
    float3 materialSpecular = float3(1.0f, 1.0f, 1.0f);

    // Phong style lighting
    float3 lightDir = normalize(lightPosition - worldPos);
    float3 viewDir = normalize(cameraPosition - worldPos);
    float3 reflectDir = reflect(-lightDir, normal);

    float3 ambient = lightIntensity * lightColor * materialAmbient;

    float diffuseFactor = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = diffuseFactor * lightIntensity * lightColor * materialDiffuse;

    float specAngle = max(dot(viewDir, reflectDir), 0.0f);
    float specularFactor = pow(specAngle, specularPower);
    float3 specular = specularFactor * lightIntensity * lightColor * materialSpecular;

    // Combine with toggles
    float3 lighting = ambient;

    if (enableDiffuse != 0)
    {
        lighting += diffuse;
    }

    if (enableSpecular != 0)
    {
        lighting += specular;
    }

    // Mode: only show albedo
    if (showAlbedoOnly != 0)
    {
        outColor[pixel] = float4(albedo, 1.0f);
    }
    else
    {
        // Multiply lighting with albedo for final color
        outColor[pixel] = float4(lighting * albedo, 1.0f);
    }
}
