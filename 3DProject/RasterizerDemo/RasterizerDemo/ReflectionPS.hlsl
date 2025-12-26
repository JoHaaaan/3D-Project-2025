struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POSITION;  // Changed from WORLD_POS to match VertexShader output
    float3 normal : NORMAL;
};

TextureCube reflectionTexture : register(t0);
sampler standardSampler : register(s0);

cbuffer CameraInfo : register(b3)
{
    float3 cameraPos;
    float padding;
};

struct PS_OUTPUT
{
    float4 Albedo : SV_Target0; // Color + ambient strength
    float4 Normal : SV_Target1; // Packed normal + specular strength
    float4 Extra : SV_Target2; // World pos + shininess
};

PS_OUTPUT main(PixelShaderInput input)
{
    PS_OUTPUT o;
    
    float3 normal = normalize(input.normal);
    float3 incomingView = normalize(input.worldPos - cameraPos); // Calculate vector that goes FROM the camera, TO the fragment being processed, in world space
    float3 reflectedView = reflect(incomingView, normal); // Use the HLSL reflect function to calculate how the incoming view vector is reflected, based on the normal of the fragment being processed
    
    float4 sampledValue = reflectionTexture.Sample(standardSampler, reflectedView); // Sample from reflectionTexture using the sampler and the reflected view vector
    
    // Output to G-Buffer format
    o.Albedo = float4(sampledValue.rgb, 0.2f); // Reflection color + ambient strength
    o.Normal = float4(normal * 0.5f + 0.5f, 0.5f); // Packed normal + specular strength
    o.Extra = float4(input.worldPos, 0.5f); // World position + shininess

    return o;
}
