#pragma once
#include <d3d11_4.h>
#include <DirectXMath.h>
#include "StructuredBufferD3D11.h"
#include "ConstantBufferD3D11.h"

using namespace DirectX;

struct Particle
{
    XMFLOAT3 position;
    float lifetime;

    XMFLOAT3 velocity;
    float maxLifetime;

    XMFLOAT4 color;
};

class ParticleSystemD3D11
{
private:
<<<<<<< Updated upstream
=======
    // Matchar ParticleGS.hlsl
    struct ParticleCameraData
    {
        XMFLOAT4X4 viewProjection;     // 64 bytes
        XMFLOAT3 cameraRight; float pad0; // 16 bytes
        XMFLOAT3 cameraUp;    float pad1; // 16 bytes
        // Total 96
    };

    // Matchar ParticleUpdateCS.hlsl (TimeBuffer)
    struct TimeData
    {
        float deltaTime;
        XMFLOAT3 emitterPosition;

        UINT emitterEnabled;
        UINT particleCount;
        float pad0;
        float pad1;
    };

    ConstantBufferD3D11 particleCameraBuffer;
    ConstantBufferD3D11 timeBuffer;

>>>>>>> Stashed changes
    StructuredBufferD3D11 particleBuffer;
    unsigned int numParticles = 0;

    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11GeometryShader* geometryShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11ComputeShader* computeShader = nullptr;

    XMFLOAT3 emitterPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT4 particleColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    bool emitterEnabled = true;

    void InitializeParticles(Particle* particles, unsigned int count);

public:
    ParticleSystemD3D11() = default;
    ParticleSystemD3D11(ID3D11Device* device,
        unsigned int numberOfParticles = 100,
        XMFLOAT3 emitterPos = XMFLOAT3(0.0f, 0.0f, 0.0f),
        XMFLOAT4 color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    ~ParticleSystemD3D11();

    ParticleSystemD3D11(const ParticleSystemD3D11& other) = delete;
    ParticleSystemD3D11& operator=(const ParticleSystemD3D11& other) = delete;
    ParticleSystemD3D11(ParticleSystemD3D11&& other) = delete;
    ParticleSystemD3D11& operator=(ParticleSystemD3D11&& other) = delete;

    void Update(ID3D11DeviceContext* context, float deltaTime);
    void Render(ID3D11DeviceContext* context, ID3D11Buffer* cameraBuffer);

    // Toggle API
    void SetEmitterEnabled(bool enabled);
    bool GetEmitterEnabled() const;

    XMFLOAT3 GetEmitterPosition() const;
    void SetEmitterPosition(const XMFLOAT3& position);

    unsigned int GetParticleCount() const;
};
