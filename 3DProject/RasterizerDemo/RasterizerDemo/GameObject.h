#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "MeshD3D11.h"
#include "ConstantBufferD3D11.h"

class GameObject
{
public:
    GameObject(const MeshD3D11* mesh);

    // Transform handling
    void SetWorldMatrix(const DirectX::XMMATRIX& world);
    DirectX::XMMATRIX GetWorldMatrix() const;

    // The Draw method handles the boilerplate of binding buffers and materials
    void Draw(ID3D11DeviceContext* context,
        ConstantBufferD3D11& matrixBuffer,
        ConstantBufferD3D11& materialBuffer,
        const DirectX::XMMATRIX& viewProjection,
        ID3D11ShaderResourceView* fallbackTexture);

private:
    const MeshD3D11* m_mesh;
    DirectX::XMFLOAT4X4 m_worldMatrix;
};