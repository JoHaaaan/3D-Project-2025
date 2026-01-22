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
    // Store configuration
    numParticles = numberOfParticles;
    emitterPosition = emitterPos;
    particleColor = color;
    
    // Create initial CPU particle data
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

    // Create constant buffers matching HLSL cbuffers (TimeBuffer + ParticleCameraBuffer)
    timeBuffer.Initialize(device, sizeof(TimeData));
    particleCameraBuffer.Initialize(device, sizeof(ParticleCameraData));

    // CPU temp data is no longer needed
    delete[] particles;
}

ParticleSystemD3D11::~ParticleSystemD3D11()
{
    // Release shader COM pointers
    if (vertexShader)
    {
        vertexShader->Release();
        vertexShader = nullptr;
    }
    if (geometryShader)
    {
        geometryShader->Release();
        geometryShader = nullptr;
    }
    if (pixelShader)
    {
        pixelShader->Release();
        pixelShader = nullptr;
    }
    if (computeShader)
    {
        computeShader->Release();
        computeShader = nullptr;
    }
}

void ParticleSystemD3D11::InitializeParticles(Particle* particles, unsigned int count)
{
    // Random number generation for particle variation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    // Fill initial particle pool
    for (unsigned int i = 0; i < count; i++)
    {
		// Spawn at emitter position
        particles[i].position = emitterPosition;

        // Random velocity range
        float velX = velocityMin.x + dist01(gen) * (velocityMax.x - velocityMin.x);
        float velY = velocityMin.y + dist01(gen) * (velocityMax.y - velocityMin.y);
        float velZ = velocityMin.z + dist01(gen) * (velocityMax.z - velocityMin.z);
        particles[i].velocity = XMFLOAT3(velX, velY, velZ);
        
        // Lifetime range
        float maxLife = 3.0f + dist01(gen) * 5.0f;
        particles[i].maxLifetime = maxLife;

        // Start at a random time so they don't all reset at once
        particles[i].lifetime = dist01(gen) * maxLife;

        // Initial color (alpha may be overwritten by CS fade)
        particles[i].color = particleColor;
    }
}

// Update Phase: Compute shader modifies particle positions, velocities, and lifetimes
void ParticleSystemD3D11::Update(ID3D11DeviceContext* context, float deltaTime)
{
	//Prepare time buffer data
    TimeData td{};
    td.deltaTime = deltaTime;
    td.emitterPosition = emitterPosition;
    td.emitterEnabled = emitterEnabled ? 1u : 0u;
    td.particleCount = numParticles;

	// Velocity range for respawning, same as in CPU init
    td.velocityMin = velocityMin;
    td.velocityMax = velocityMax;
    td.pad2 = 0.0f;
    td.pad3 = 0.0f;

    
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

    // SRV to Vertex Shader
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