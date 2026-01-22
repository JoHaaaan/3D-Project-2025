#include "ParticleSystemD3D11.h"
#include "ShaderLoader.h"
#include <cmath>
#include <random>

// ========================================
// PARTICLE SYSTEM - GPU-Based Simulation
// ========================================
// Demonstrates compute shader particle updates with geometry shader billboarding
// Key techniques: Structured buffers (SRV+UAV), compute shaders, geometry shader expansion

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

    // Structured buffer: readable (SRV) in vertex shader, writable (UAV) in compute shader
    particleBuffer.Initialize(device, sizeof(Particle), numberOfParticles,
        particles, false, true);

    // Load specialized particle rendering pipeline
    vertexShader = ShaderLoader::CreateVertexShader(device, "ParticleVS.cso", nullptr);
    geometryShader = ShaderLoader::CreateGeometryShader(device, "ParticleGS.cso");
    pixelShader = ShaderLoader::CreatePixelShader(device, "ParticlePS.cso");
    computeShader = ShaderLoader::CreateComputeShader(device, "ParticleUpdateCS.cso");

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
}

void ParticleSystemD3D11::InitializeParticles(Particle* particles, unsigned int count)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    // Initialize each particle with random velocity and lifetime
    for (unsigned int i = 0; i < count; i++)
    {
        particles[i].position = emitterPosition;

        float velX = -2.0f + dist01(gen) * 4.0f;
        float velY = 2.0f + dist01(gen) * 2.0f;
        float velZ = -2.0f + dist01(gen) * 4.0f;
        particles[i].velocity = XMFLOAT3(velX, velY, velZ);

        float maxLife = 3.0f + dist01(gen) * 5.0f;
        particles[i].maxLifetime = maxLife;
        particles[i].lifetime = dist01(gen) * maxLife;
        particles[i].color = particleColor;
    }
}

// Update Phase: Compute shader modifies particle positions, velocities, and lifetimes
void ParticleSystemD3D11::Update(ID3D11DeviceContext* context, float deltaTime)
{
    TimeData td{};
    td.deltaTime = deltaTime;
    td.emitterPosition = emitterPosition;
    td.emitterEnabled = emitterEnabled ? 1u : 0u;
    td.particleCount = numParticles;
    td.pad0 = td.pad1 = 0.0f;

    timeBuffer.UpdateBuffer(context, &td);

    context->CSSetShader(computeShader, nullptr, 0);

    ID3D11Buffer* timeBuf = timeBuffer.GetBuffer();
    context->CSSetConstantBuffers(0, 1, &timeBuf);

    // Bind as UAV (unordered access) for read-write operations in compute shader
    ID3D11UnorderedAccessView* uav = particleBuffer.GetUAV();
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

    // Dispatch compute shader (32 threads per group)
    unsigned int numGroups = static_cast<unsigned int>(std::ceil(numParticles / 32.0f));
    context->Dispatch(numGroups, 1, 1);

    // Unbind UAV before using as SRV in rendering
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

    context->CSSetShader(nullptr, nullptr, 0);
}

// Render Phase: Vertex shader reads particles, geometry shader creates billboards
void ParticleSystemD3D11::Render(ID3D11DeviceContext* context, const CameraD3D11& camera)
{
    if (!vertexShader || !geometryShader || !pixelShader)
        return;

    // Disable tessellation stages for this pipeline
    context->HSSetShader(nullptr, nullptr, 0);
    context->DSSetShader(nullptr, nullptr, 0);

    // Prepare camera data for billboard orientation
    ParticleCameraData cd{};
    cd.pad0 = 0.0f;
    cd.pad1 = 0.0f;

    XMFLOAT4X4 vp = camera.GetViewProjectionMatrix();
    XMMATRIX VPm = XMLoadFloat4x4(&vp);
    VPm = XMMatrixTranspose(VPm);
    XMStoreFloat4x4(&cd.viewProjection, VPm);

    // Camera axes for billboard alignment
    cd.cameraRight = camera.GetRight();
    cd.cameraUp = camera.GetUp();

    particleCameraBuffer.UpdateBuffer(context, &cd);

    // Bind particle buffer as SRV (read-only) for vertex shader
    ID3D11ShaderResourceView* srv = particleBuffer.GetSRV();
    if (!srv)
        return;

    // Point list topology: each particle is a single point
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

    UINT stride = 0, offset = 0;
    ID3D11Buffer* nullVB = nullptr;
    context->IASetVertexBuffers(0, 1, &nullVB, &stride, &offset);

    context->VSSetShader(vertexShader, nullptr, 0);
    context->GSSetShader(geometryShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);

    context->VSSetShaderResources(0, 1, &srv);

    ID3D11Buffer* camBuf = particleCameraBuffer.GetBuffer();
    context->GSSetConstantBuffers(0, 1, &camBuf);

    // Draw all particles as points (geometry shader expands each to quad)
    context->Draw(numParticles, 0);

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