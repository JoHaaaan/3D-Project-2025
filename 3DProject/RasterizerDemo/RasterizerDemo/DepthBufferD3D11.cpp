#include "DepthBufferD3D11.h"

DepthBufferD3D11::DepthBufferD3D11(ID3D11Device* device, UINT width, UINT height, bool hasSRV)
{
	Initialize(device, width, height, hasSRV, 1);
}

DepthBufferD3D11::~DepthBufferD3D11()
{
	if (srv)
		srv->Release();

	for (auto& dsv : depthStencilViews)
	{
		if (dsv)
			dsv->Release();
	}

	if (texture)
		texture->Release();
}

void DepthBufferD3D11::Initialize(ID3D11Device* device, UINT width, UINT height,
	bool hasSRV, UINT arraySize)
{
	// Texture description
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = arraySize;
	texDesc.Format = hasSRV ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	if (hasSRV)
		texDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	device->CreateTexture2D(&texDesc, nullptr, &texture);

	// Create depth stencil views
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = (arraySize > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DARRAY : D3D11_DSV_DIMENSION_TEXTURE2D;

	depthStencilViews.resize(arraySize);
	for (UINT i = 0; i < arraySize; ++i)
	{
		if (arraySize > 1)
		{
			dsvDesc.Texture2DArray.MipSlice = 0;
			dsvDesc.Texture2DArray.FirstArraySlice = i;
			dsvDesc.Texture2DArray.ArraySize = 1;
		}
		else
		{
			dsvDesc.Texture2D.MipSlice = 0;
		}

		device->CreateDepthStencilView(texture, &dsvDesc, &depthStencilViews[i]);
	}

	// Create shader resource view if requested
	if (hasSRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		if (arraySize > 1)
		{
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
			srvDesc.Texture2DArray.MipLevels = 1;
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.ArraySize = arraySize;
		}
		else
		{
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;
		}

		device->CreateShaderResourceView(texture, &srvDesc, &srv);
	}
}

ID3D11DepthStencilView* DepthBufferD3D11::GetDSV(UINT arrayIndex) const
{
	if (arrayIndex < depthStencilViews.size())
		return depthStencilViews[arrayIndex];
	return nullptr;
}

ID3D11ShaderResourceView* DepthBufferD3D11::GetSRV() const
{
	return srv;
}
