#pragma once
#include <d3d11_4.h>
#include <DirectXMath.h>
#include "StructuredBufferD3D11.h"
#include "ConstantBufferD3D11.h"

using namespace DirectX;

struct Particle
{
    XMFLOAT3 position;      // 12 bytes
    float lifetime;         // 4 bytes

    XMFLOAT3 velocity;      // 12 bytes
    float maxLifetime;      // 4 bytes

    XMFLOAT4 color;         // 16 bytes
    // Total: 48 bytes (perfekt 16-byte aligned!)
};

class ParticleSystemD3D11
{
private:
    StructuredBufferD3D11 particleBuffer;
    unsigned int numParticles;

    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11GeometryShader* geometryShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11ComputeShader* computeShader = nullptr;

    ConstantBufferD3D11 timeBuffer;

    XMFLOAT3 emitterPosition;
    XMFLOAT4 particleColor;

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

    XMFLOAT3 GetEmitterPosition() const;
    void SetEmitterPosition(const XMFLOAT3& position);
    unsigned int GetParticleCount() const;
};