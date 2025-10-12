#include "RenderTargetD3D11.h"

RenderTargetD3D11::~RenderTargetD3D11()
{
	if (srv)
		srv->Release();
	if (rtv)
		rtv->Release();
	if (texture)
		texture->Release();
}

void RenderTargetD3D11::Initialize(ID3D11Device* device, UINT width, UINT height,
	DXGI_FORMAT format, bool hasSRV)
{
	// Texture description
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
	if (hasSRV)
		texDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	device->CreateTexture2D(&texDesc, nullptr, &texture);

	// Create render target view
	device->CreateRenderTargetView(texture, nullptr, &rtv);

	// Create shader resource view if requested
	if (hasSRV)
	{
		device->CreateShaderResourceView(texture, nullptr, &srv);
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
