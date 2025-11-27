#include "GBufferD3D11.h"

void GBufferD3D11::Initialize(ID3D11Device* device,
    UINT width,
    UINT height)
{
    // Färg/Albedo (RGBA8 räcker fint)
    albedoRT.Initialize(device,
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        true); // hasSRV = true, vi vill läsa i lighting-pass

    // Normal – också RGBA8, vi packar normal i RGB (och ev. något i A)
    normalRT.Initialize(device,
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        true);

    // Specular/Gloss – också RGBA8 för enkelhetens skull
    specRT.Initialize(device,
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        true);
}

void GBufferD3D11::SetAsRenderTargets(ID3D11DeviceContext* context,
    ID3D11DepthStencilView* dsv)
{
    ID3D11RenderTargetView* rtvs[3] =
    {
        albedoRT.GetRTV(),
        normalRT.GetRTV(),
        specRT.GetRTV()
    };

    context->OMSetRenderTargets(3, rtvs, dsv);
}

void GBufferD3D11::Clear(ID3D11DeviceContext* context,
    const float clearColor[4])
{
    context->ClearRenderTargetView(albedoRT.GetRTV(), clearColor);
    context->ClearRenderTargetView(normalRT.GetRTV(), clearColor);
    context->ClearRenderTargetView(specRT.GetRTV(), clearColor);
}

ID3D11ShaderResourceView* GBufferD3D11::GetAlbedoSRV() const
{
    return albedoRT.GetSRV();
}

ID3D11ShaderResourceView* GBufferD3D11::GetNormalSRV() const
{
    return normalRT.GetSRV();
}

ID3D11ShaderResourceView* GBufferD3D11::GetSpecSRV() const
{
    return specRT.GetSRV();
}
