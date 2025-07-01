// RenderHelper.h
#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;



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
