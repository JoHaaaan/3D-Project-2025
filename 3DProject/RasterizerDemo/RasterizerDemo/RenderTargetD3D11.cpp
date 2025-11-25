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

void RenderTargetD3D11::Initialize(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format, bool hasSRV)
{

}

ID3D11RenderTargetView* RenderTargetD3D11::GetRTV() const
{
	return nullptr;
}

ID3D11ShaderResourceView* RenderTargetD3D11::GetSRV() const
{
	return nullptr;
}
