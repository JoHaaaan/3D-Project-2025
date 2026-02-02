#pragma once

#include <DirectXMath.h>

// Light data structure matching GPU layout in compute shader
struct LightData
{
    DirectX::XMFLOAT4X4 viewProj;
    DirectX::XMFLOAT3 position;
    float intensity;
    DirectX::XMFLOAT3 direction;
    float range;
    DirectX::XMFLOAT3 color;
    float spotAngle;
    int type;           // 0 = Directional, 1 = Spotlight
    int enabled;
    float padding[2];
};

// Matrix pair for transforming vertices (world and view-projection)
struct MatrixPair
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 viewProj;
};

// Global view-projection matrix (updated by camera each frame)
extern DirectX::XMMATRIX VIEW_PROJ;
