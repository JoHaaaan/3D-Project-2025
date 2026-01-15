#include "ParticleSystemD3D11.h"
#include "ShaderLoader.h"
#include <cmath>
#include <random>

ParticleSystemD3D11::ParticleSystemD3D11(ID3D11Device* device,
    unsigned int numberOfParticles,
    XMFLOAT3 emitterPos,
    XMFLOAT4 color)
{
    // Spara parametrar till member variabler
    numParticles = numberOfParticles;
    emitterPosition = emitterPos;
    particleColor = color;

    // Skapa temporary particle array på CPU
    Particle* particles = new Particle[numberOfParticles];

    // Fyll arrayen med startdata
    InitializeParticles(particles, numberOfParticles);

    // Skapa particle buffer på GPU med både SRV och UAV
    particleBuffer.Initialize(device, sizeof(Particle), numberOfParticles,
        particles, false, true);
    // false = inte dynamic (GPU uppdaterar via compute shader)
    // true = skapa UAV (compute shader behöver write access)

    // Ladda shaders från compiled .cso filer
    vertexShader = ShaderLoader::CreateVertexShader(device, "ParticleVS.cso", nullptr);
    geometryShader = ShaderLoader::CreateGeometryShader(device, "ParticleGS.cso");
    pixelShader = ShaderLoader::CreatePixelShader(device, "ParticlePS.cso");
    computeShader = ShaderLoader::CreateComputeShader(device, "ParticleUpdateCS.cso");

    // Skapa constant buffer för delta time (1 float)
    timeBuffer.Initialize(device, sizeof(float));

    // Rensa temporary array
    delete[] particles;
}

ParticleSystemD3D11::~ParticleSystemD3D11()
{
    if (vertexShader) vertexShader->Release();
    if (geometryShader) geometryShader->Release();
    if (pixelShader) pixelShader->Release();
    if (computeShader) computeShader->Release();
}

void ParticleSystemD3D11::InitializeParticles(Particle* particles, unsigned int count)
{
    // Sätt upp slumpgenerator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // Fyll varje partikel med startdata
    for (unsigned int i = 0; i < count; i++)
    {
        // Position: alla startar vid emittern
        particles[i].position = emitterPosition;

        // Velocity: slumpmässig riktning
        // Exempel för snö/regn som faller neråt med lite sidledes drift
        float velX = -2.0f + dist(gen) * 4.0f;      // -2 till +2
        float velY = -5.0f + dist(gen) * 4.0f;      // -5 till -1 (neråt)
        float velZ = -2.0f + dist(gen) * 4.0f;      // -2 till +2
        particles[i].velocity = XMFLOAT3(velX, velY, velZ);

        // MaxLifetime: slumpmässig livslängd mellan 3-8 sekunder
        float maxLife = 3.0f + dist(gen) * 5.0f;
        particles[i].maxLifetime = maxLife;

        // Lifetime: starta med slumpmässig livstid för variation
        // Detta gör att inte alla partiklar "dör" samtidigt
        particles[i].lifetime = dist(gen) * maxLife;

        // Color: använd färgen från konstruktor parameter
        particles[i].color = particleColor;
    }
}

void ParticleSystemD3D11::Update(ID3D11DeviceContext* context, float deltaTime)
{
    // Uppdatera time constant buffer med delta time
    timeBuffer.UpdateBuffer(context, &deltaTime);

    // Binda compute shader
    context->CSSetShader(computeShader, nullptr, 0);

    // Binda time buffer till compute shader (register b0)
    ID3D11Buffer* timeBuf = timeBuffer.GetBuffer();
    context->CSSetConstantBuffers(0, 1, &timeBuf);

    // Binda particle buffer UAV till compute shader (register u0)
    ID3D11UnorderedAccessView* uav = particleBuffer.GetUAV();
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

    // Dispatch compute shader
    // Beräkna antal grupper: ceil(numParticles / 32.0)
    // 32 threads per grupp (som i shader: [numthreads(32,1,1)])
    unsigned int numGroups = static_cast<unsigned int>(std::ceil(numParticles / 32.0f));
    context->Dispatch(numGroups, 1, 1);

    // VIKTIGT: Unbind UAV innan rendering!
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
}

void ParticleSystemD3D11::Render(ID3D11DeviceContext* context, ID3D11Buffer* cameraBuffer)
{
    // Sätt input layout till nullptr (vertex pulling, ingen layout!)
    context->IASetInputLayout(nullptr);

    // Sätt topology till POINTLIST (bara punkter, inte trianglar)
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

    // Binda vertex shader
    context->VSSetShader(vertexShader, nullptr, 0);

    // Binda particle buffer SRV till vertex shader (register t0)
    ID3D11ShaderResourceView* srv = particleBuffer.GetSRV();
    context->VSSetShaderResources(0, 1, &srv);

    // Binda geometry shader
    context->GSSetShader(geometryShader, nullptr, 0);

    // Binda camera constant buffer till geometry shader (register b0)
    context->GSSetConstantBuffers(0, 1, &cameraBuffer);

    // Binda pixel shader
    context->PSSetShader(pixelShader, nullptr, 0);

    // Draw call - rita numParticles punkter
    context->Draw(numParticles, 0);

    // VIKTIGT: Unbind geometry shader (annars kan det störa annan rendering)
    context->GSSetShader(nullptr, nullptr, 0);

    // VIKTIGT: Unbind particle buffer SRV (annars konflikt med Update nästa frame)
    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->VSSetShaderResources(0, 1, &nullSRV);
}

XMFLOAT3 ParticleSystemD3D11::GetEmitterPosition() const
{
    return emitterPosition;
}

void ParticleSystemD3D11::SetEmitterPosition(const XMFLOAT3& position)
{
    emitterPosition = position;
}

unsigned int ParticleSystemD3D11::GetParticleCount() const
{
    return numParticles;
}