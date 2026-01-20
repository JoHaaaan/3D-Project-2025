#include "RenderTargetD3D11.h"

RenderTargetD3D11::~RenderTargetD3D11()
{
	if (rtv)
	{
		rtv->Release();
		rtv = nullptr;
	}

	if (srv)
	{
		srv->Release();
		srv = nullptr;
	}
	if (texture)
	{
		texture->Release();
		texture = nullptr;
	}
}

void RenderTargetD3D11::Initialize(ID3D11Device* device,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    bool hasSRV)
{
    // Släpp gamla resurser om Initialize kallas igen
    if (rtv)
    {
        rtv->Release();
        rtv = nullptr;
    }

    if (srv)
    {
        srv->Release();
        srv = nullptr;
    }

    if (texture)
    {
        texture->Release();
        texture = nullptr;
    }

    if (!device)
        return;

    // 1) Beskriv själva texturen vi ska rita till
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    if (hasSRV)
        texDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

    // 2) Skapa texturen på GPU:n
    HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &texture);
    if (FAILED(hr))
        return;

    // 3) Skapa RTV så vi kan rita till texturen
    hr = device->CreateRenderTargetView(texture, nullptr, &rtv);
    if (FAILED(hr))
    {
        texture->Release();
        texture = nullptr;
        return;
    }

    // 4) Skapa SRV om vi vill läsa från texturen i en shader
    if (hasSRV)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        hr = device->CreateShaderResourceView(texture, &srvDesc, &srv);
        if (FAILED(hr))
        {
            srv = nullptr; 
        }
    }
}

ID3D11RenderTargetView* RenderTargetD3D11::GetRTV() const
{
    return rtv;
}

ID3D11ShaderResourceView* RenderTargetD3D11::GetSRV() const
{
    return srv;
}
