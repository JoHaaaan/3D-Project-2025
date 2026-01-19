#include "ParticleSystemD3D11.h"
#include "ShaderLoader.h"
#include <cmath>
#include <random>

ParticleSystemD3D11::ParticleSystemD3D11(ID3D11Device* device,
    unsigned int numberOfParticles,
    XMFLOAT3 emitterPos,
    XMFLOAT4 color)
{
    numParticles = numberOfParticles;
    emitterPosition = emitterPos;
    particleColor = color;

    Particle* particles = new Particle[numberOfParticles];
    InitializeParticles(particles, numberOfParticles);

    // GPU buffer med SRV + UAV
    particleBuffer.Initialize(device, sizeof(Particle), numberOfParticles,
        particles, false, true);

    // Shaders
    vertexShader = ShaderLoader::CreateVertexShader(device, "ParticleVS.cso", nullptr);
    geometryShader = ShaderLoader::CreateGeometryShader(device, "ParticleGS.cso");
    pixelShader = ShaderLoader::CreatePixelShader(device, "ParticlePS.cso");
    computeShader = ShaderLoader::CreateComputeShader(device, "ParticleUpdateCS.cso");

    // Skapa constant buffer for delta time (1 float)
    timeBuffer.Initialize(device, sizeof(float));

    // Rensa temporary array
    // Constant buffers
    timeBuffer.Initialize(device, sizeof(TimeData));
    particleCameraBuffer.Initialize(device, sizeof(ParticleCameraData));

    delete[] particles;
}

ParticleSystemD3D11::~ParticleSystemD3D11()
{
    if (vertexShader) { vertexShader->Release(); vertexShader = nullptr; }
    if (geometryShader) { geometryShader->Release(); geometryShader = nullptr; }
    if (pixelShader) { pixelShader->Release(); pixelShader = nullptr; }
    if (computeShader) { computeShader->Release(); computeShader = nullptr; }

    // particleBuffer, timeBuffer, particleCameraBuffer slapps av sina destructors
}

void ParticleSystemD3D11::InitializeParticles(Particle* particles, unsigned int count)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    for (unsigned int i = 0; i < count; i++)
    {
        particles[i].position = emitterPosition;

        float velX = -2.0f + dist01(gen) * 4.0f;
        float velY = 2.0f + dist01(gen) * 2.0f;
        float velZ = -2.0f + dist01(gen) * 4.0f;
        particles[i].velocity = XMFLOAT3(velX, velY, velZ);

        float maxLife = 3.0f + dist01(gen) * 5.0f;
        particles[i].maxLifetime = maxLife;

        // Ge variation i startlage:
        // Om emitter ar pa vill du ofta starta med spridning, men:
        // Vi haller dem aktiva fran start.
        particles[i].lifetime = dist01(gen) * maxLife;

        particles[i].color = particleColor;
    }
}

void ParticleSystemD3D11::Update(ID3D11DeviceContext* context, float deltaTime)
{
    // Bygg TimeData exakt som HLSL TimeBuffer
    TimeData td{};
    td.deltaTime = deltaTime;
    td.emitterPosition = emitterPosition;
    td.emitterEnabled = emitterEnabled ? 1u : 0u;
    td.particleCount = numParticles;
    td.pad0 = td.pad1 = 0.0f;

    timeBuffer.UpdateBuffer(context, &td);

    // Bind compute shader
    context->CSSetShader(computeShader, nullptr, 0);

    // Bind time buffer (b0)
    ID3D11Buffer* timeBuf = timeBuffer.GetBuffer();
    context->CSSetConstantBuffers(0, 1, &timeBuf);

    // Bind UAV (u0)
    ID3D11UnorderedAccessView* uav = particleBuffer.GetUAV();
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

    // Dispatch
    unsigned int numGroups = static_cast<unsigned int>(std::ceil(numParticles / 32.0f));
    context->Dispatch(numGroups, 1, 1);

    // Unbind UAV (viktigt innan render)
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

    // Unbind CS (valfritt men clean)
    context->CSSetShader(nullptr, nullptr, 0);
}

void ParticleSystemD3D11::Render(ID3D11DeviceContext* context, ID3D11Buffer* cameraBuffer)
{
    if (!vertexShader || !geometryShader || !pixelShader)
        return;

    // Unbind tess stages sa GS funkar rent
    context->HSSetShader(nullptr, nullptr, 0);
    context->DSSetShader(nullptr, nullptr, 0);

    // ---- Camera buffer till GS ----
    ParticleCameraData cd{};
    cd.pad0 = 0.0f;
    cd.pad1 = 0.0f;

    // Paket A: shaders kar mul(v, M) => CPU maste skicka TRANSPOSEAD matris
    XMFLOAT4X4 vp = camera.GetViewProjectionMatrix();            // (icke-transponerad fran kameran)
    XMMATRIX VPm = XMLoadFloat4x4(&vp);
    VPm = XMMatrixTranspose(VPm);                                 // TRANSPOSE har
    XMStoreFloat4x4(&cd.viewProjection, VPm);

    cd.cameraRight = camera.GetRight();
    cd.cameraUp = camera.GetUp();

    particleCameraBuffer.UpdateBuffer(context, &cd);

    // SRV till VS
    ID3D11ShaderResourceView* srv = particleBuffer.GetSRV();
    if (!srv)
        return;
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

    UINT stride = 0, offset = 0;
    ID3D11Buffer* nullVB = nullptr;
    context->IASetVertexBuffers(0, 1, &nullVB, &stride, &offset);

    context->VSSetShader(vertexShader, nullptr, 0);
    context->GSSetShader(geometryShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);

    // VS: particle buffer SRV
    context->VSSetShaderResources(0, 1, &srv);


    // Binda geometry shader
    context->GSSetShader(geometryShader, nullptr, 0);

    // Binda camera constant buffer till geometry shader (register b0)
    context->GSSetConstantBuffers(0, 1, &cameraBuffer);

    // GS: camera buffer
    ID3D11Buffer* camBuf = particleCameraBuffer.GetBuffer();
    context->GSSetConstantBuffers(0, 1, &camBuf);


    context->Draw(numParticles, 0);

    // Cleanup: unbind
    context->GSSetShader(nullptr, nullptr, 0);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->VSSetShaderResources(0, 1, &nullSRV);
}


void ParticleSystemD3D11::SetEmitterEnabled(bool enabled)
{
    emitterEnabled = enabled;
}

bool ParticleSystemD3D11::GetEmitterEnabled() const
{
    return emitterEnabled;
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
