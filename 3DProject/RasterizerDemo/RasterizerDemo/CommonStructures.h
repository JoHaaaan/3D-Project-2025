#pragma once

#include <DirectXMath.h>

struct LightData
{
    DirectX::XMFLOAT4X4 viewProj;
    DirectX::XMFLOAT3 position;
    float intensity;
    DirectX::XMFLOAT3 direction;
    float range;
    DirectX::XMFLOAT3 color;
    float spotAngle;
    int type;
    int enabled;
    float padding[2];
};

struct MatrixPair
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 viewProj;
};

extern DirectX::XMMATRIX VIEW_PROJ;
