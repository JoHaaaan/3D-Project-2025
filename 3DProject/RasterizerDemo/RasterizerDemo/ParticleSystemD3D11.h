#pragma once
#include <d3d11_4.h>
#include <DirectXMath.h>
#include "StructuredBufferD3D11.h"
#include "ConstantBufferD3D11.h"
#include "CameraD3D11.h"

using namespace DirectX;

struct Particle
{
    XMFLOAT3 position;
    float lifetime;
    XMFLOAT3 velocity;
    float maxLifetime;
    XMFLOAT4 color;
};
// 48 bytes

class ParticleSystemD3D11
{

private:

	// Matches ParticleGS ParticleCameraBuffer
    struct ParticleCameraData
    {
        XMFLOAT4X4 viewProjection;        
        XMFLOAT3 cameraRight; 
        float pad0;
        XMFLOAT3 cameraUp;    
        float pad1;
    };
	// 96 bytes, padding for 16-byte aligned

    // Matches ParticleUpdateCS Timebuffer
    struct TimeData
    {
        float deltaTime;
        XMFLOAT3 emitterPosition;
        UINT emitterEnabled;
        UINT particleCount;
        float pad0;
        float pad1;
    };
	// 32 bytes, padding for 16-byte aligned


    // GPU resources owned/managed by this particle system.
    ConstantBufferD3D11 particleCameraBuffer;
    ConstantBufferD3D11 timeBuffer;
    StructuredBufferD3D11 particleBuffer;
    
    unsigned int numParticles = 0;

	// Shaders used by the particle pipeline
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11GeometryShader* geometryShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11ComputeShader* computeShader = nullptr;

	// Particle system properties
    XMFLOAT3 emitterPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT4 particleColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    bool emitterEnabled = true;

    // Initialize particle buffer with random starting values
    void InitializeParticles(Particle* particles, unsigned int count);

public:

	// Constructors and destructors
    ParticleSystemD3D11() = default;
    ParticleSystemD3D11(ID3D11Device* device,
        unsigned int numberOfParticles = 100,
        XMFLOAT3 emitterPos = XMFLOAT3(0.0f, 0.0f, 0.0f),
        XMFLOAT4 color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    ~ParticleSystemD3D11();

    // Disable copy/move to prevent shallow sharing of GPU resources/COM pointers
    ParticleSystemD3D11(const ParticleSystemD3D11& other) = delete;
    ParticleSystemD3D11& operator=(const ParticleSystemD3D11& other) = delete;
    ParticleSystemD3D11(ParticleSystemD3D11&& other) = delete;
    ParticleSystemD3D11& operator=(ParticleSystemD3D11&& other) = delete;

	// Update and render
    void Update(ID3D11DeviceContext* context, float deltaTime);
    void Render(ID3D11DeviceContext* context, const CameraD3D11& camera);

	// Toggles if new particles are emitted/spawned
    void SetEmitterEnabled(bool enabled);
    bool GetEmitterEnabled() const;

    XMFLOAT3 GetEmitterPosition() const;
    void SetEmitterPosition(const XMFLOAT3& position);
};
