// RenderHelper.h
#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;

// In RenderHelper.h

struct LightData
{
    DirectX::XMFLOAT4X4 viewProj; // 64 bytes - The light's view-projection matrix
    DirectX::XMFLOAT3 position;   // 12 bytes
    float intensity;              // 4 bytes
    DirectX::XMFLOAT3 direction;  // 12 bytes
    float range;                  // 4 bytes
    DirectX::XMFLOAT3 color;      // 12 bytes
    float spotAngle;              // 4 bytes
    int type;                     // 4 bytes (0 = Directional, 1 = Spot)
    int enabled;                  // 4 bytes
    float padding[2];             // 8 bytes (Total size = 128 bytes, 16-byte aligned)
};

struct MatrixPair {
    XMFLOAT4X4 world;
    XMFLOAT4X4 viewProj;
};

extern XMMATRIX VIEW_PROJ;

void Render(
    ID3D11DeviceContext* immediateContext,
    ID3D11RenderTargetView* rtv,
    ID3D11DepthStencilView* dsView,
    D3D11_VIEWPORT& viewport,
    ID3D11VertexShader* vShader,
    ID3D11PixelShader* pShader,
    ID3D11InputLayout* inputLayout,
    ID3D11Buffer* vertexBuffer,
    ID3D11Buffer* constantBuffer,
    ID3D11ShaderResourceView* textureView,
    ID3D11SamplerState* samplerState,
    const XMMATRIX& worldMatrix);
