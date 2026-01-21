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

    // Initialize GPU buffer with SRV + UAV
    particleBuffer.Initialize(device, sizeof(Particle), numberOfParticles,
        particles, false, true);

	// Shaders for the particle system
    vertexShader = ShaderLoader::CreateVertexShader(device, "ParticleVS.cso", nullptr);
    geometryShader = ShaderLoader::CreateGeometryShader(device, "ParticleGS.cso");
    pixelShader = ShaderLoader::CreatePixelShader(device, "ParticlePS.cso");
    computeShader = ShaderLoader::CreateComputeShader(device, "ParticleUpdateCS.cso");

	// Constant buffers for time and camera data
    timeBuffer.Initialize(device, sizeof(TimeData));
    particleCameraBuffer.Initialize(device, sizeof(ParticleCameraData));

    delete[] particles;
}

ParticleSystemD3D11::~ParticleSystemD3D11()
{
	//Destructor for releasing COM pointers
    if (vertexShader) { vertexShader->Release(); vertexShader = nullptr; }
    if (geometryShader) { geometryShader->Release(); geometryShader = nullptr; }
    if (pixelShader) { pixelShader->Release(); pixelShader = nullptr; }
    if (computeShader) { computeShader->Release(); computeShader = nullptr; }
}

void ParticleSystemD3D11::InitializeParticles(Particle* particles, unsigned int count)
{
	//For random generation
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
        particles[i].lifetime = dist01(gen) * maxLife;
        particles[i].color = particleColor;
    }
}

void ParticleSystemD3D11::Update(ID3D11DeviceContext* context, float deltaTime)
{
    // Build TimeData exactly like HLSL TimeBuffer
    TimeData td{};
    td.deltaTime = deltaTime;
    td.emitterPosition = emitterPosition;
    td.emitterEnabled = emitterEnabled ? 1u : 0u;
    td.particleCount = numParticles;
    td.pad0 = td.pad1 = 0.0f;

    timeBuffer.UpdateBuffer(context, &td);

    // Bind Compute Shader
    context->CSSetShader(computeShader, nullptr, 0);

    // Bind time buffer
    ID3D11Buffer* timeBuf = timeBuffer.GetBuffer();
    context->CSSetConstantBuffers(0, 1, &timeBuf);

    // Bind UAV
    ID3D11UnorderedAccessView* uav = particleBuffer.GetUAV();
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

    // Dispatch
    unsigned int numGroups = static_cast<unsigned int>(std::ceil(numParticles / 32.0f));
    context->Dispatch(numGroups, 1, 1);

    // Unbind UAV (before render)
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

    // Unbind Compute Shader
    context->CSSetShader(nullptr, nullptr, 0);
}

void ParticleSystemD3D11::Render(ID3D11DeviceContext* context, const CameraD3D11& camera)
{
    if (!vertexShader || !geometryShader || !pixelShader)
        return;

	// Unbind tessellation stages so Geometry Shader works
    context->HSSetShader(nullptr, nullptr, 0);
    context->DSSetShader(nullptr, nullptr, 0);

	// Camera buffer to Geometry Shader
    ParticleCameraData cd{};
    cd.pad0 = 0.0f;
    cd.pad1 = 0.0f;

	// Transpose matrix for HLSL
    XMFLOAT4X4 vp = camera.GetViewProjectionMatrix();
    XMMATRIX VPm = XMLoadFloat4x4(&vp);
    VPm = XMMatrixTranspose(VPm);
    XMStoreFloat4x4(&cd.viewProjection, VPm);

    cd.cameraRight = camera.GetRight();
    cd.cameraUp = camera.GetUp();

    particleCameraBuffer.UpdateBuffer(context, &cd);

    // SRV till Vertex Shader
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