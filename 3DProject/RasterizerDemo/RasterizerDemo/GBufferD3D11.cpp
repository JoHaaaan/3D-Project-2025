#include "GBufferD3D11.h"

void GBufferD3D11::Initialize(ID3D11Device* device,
    UINT width,
    UINT height)
{
    albedoRT.Initialize(device,
        width,
        height,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        true);

    // Normal
    normalRT.Initialize(device,
        width,
        height,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        true);

    // Position
    positionRT.Initialize(device,
        width,
        height,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        true);
}

void GBufferD3D11::SetAsRenderTargets(ID3D11DeviceContext* context,
    ID3D11DepthStencilView* dsv)
{
    ID3D11RenderTargetView* rtvs[3] =
    {
        albedoRT.GetRTV(),
        normalRT.GetRTV(),
        positionRT.GetRTV()
    };

    context->OMSetRenderTargets(3, rtvs, dsv);
}

void GBufferD3D11::Clear(ID3D11DeviceContext* context,
    const float clearColor[4])
{
    context->ClearRenderTargetView(albedoRT.GetRTV(), clearColor);
    context->ClearRenderTargetView(normalRT.GetRTV(), clearColor);
    context->ClearRenderTargetView(positionRT.GetRTV(), clearColor);
}

ID3D11ShaderResourceView* GBufferD3D11::GetAlbedoSRV() const
{
    return albedoRT.GetSRV();
}

ID3D11ShaderResourceView* GBufferD3D11::GetNormalSRV() const
{
    return normalRT.GetSRV();
}

ID3D11ShaderResourceView* GBufferD3D11::GetPositionSRV() const
{
    return positionRT.GetSRV();
}
