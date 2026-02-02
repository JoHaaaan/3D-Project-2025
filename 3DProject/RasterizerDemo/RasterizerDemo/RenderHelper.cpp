#include "RenderHelper.h"
#include "PipelineHelper.h"


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
    const XMMATRIX& worldMatrix)
{
    extern DirectX::XMMATRIX VIEW_PROJ;

    float clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
    immediateContext->ClearRenderTargetView(rtv, clearColor);
    immediateContext->ClearDepthStencilView(dsView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

    XMMATRIX worldT = XMMatrixTranspose(worldMatrix);
    XMMATRIX viewProjT = XMMatrixTranspose(VIEW_PROJ);

    D3D11_MAPPED_SUBRESOURCE mapped;
    immediateContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

    MatrixPair data;
    XMStoreFloat4x4(&data.world, worldT);
    XMStoreFloat4x4(&data.viewProj, viewProjT);
    memcpy(mapped.pData, &data, sizeof(MatrixPair));
}
