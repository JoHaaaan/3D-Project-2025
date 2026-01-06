#pragma once
#include <d3d11_4.h>
#include <DirectXMath.h>
#include "StructuredBufferD3D11.h"
#include "ShaderD3D11.h"
#include "ConstantBufferD3D11.h"

struct Particle
{
    DirectX::XMFLOAT3 position;
    float lifetime;
    DirectX::XMFLOAT3 velocity;
    float maxLifetime;
    DirectX::XMFLOAT4 color;
};

class ParticleSystemD3D11
{
private:
    StructuredBufferD3D11 particleBuffer;



public:

};