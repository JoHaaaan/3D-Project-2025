#include "DepthBufferD3D11.h"

DepthBufferD3D11::DepthBufferD3D11(ID3D11Device* device, UINT width, UINT height, bool hasSRV)
{
	Initialize(device, width, height, hasSRV, 1);
}

DepthBufferD3D11::~DepthBufferD3D11()
{
    for (size_t i = 0; i < depthStencilViews.size(); ++i)
    {
        if (depthStencilViews[i])
        {
            depthStencilViews[i]->Release();
            depthStencilViews[i] = nullptr;
        }
    }
    depthStencilViews.clear();

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


void DepthBufferD3D11::Initialize(ID3D11Device* device, UINT width, UINT height, bool hasSRV, UINT arraySize)
{
    for (size_t i = 0; i < depthStencilViews.size(); ++i)
    {
        if (depthStencilViews[i])
        {
            depthStencilViews[i]->Release();
            depthStencilViews[i] = nullptr;
        }
    }
    depthStencilViews.clear();

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

    if (arraySize == 0)
        arraySize = 1;

    DXGI_FORMAT texFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DXGI_FORMAT dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;

    UINT bindFlags = D3D11_BIND_DEPTH_STENCIL;

    if (hasSRV)
    {
        // Use typeless format to enable both DSV and SRV creation
        texFormat = DXGI_FORMAT_R24G8_TYPELESS;
        dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        bindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = arraySize;
    texDesc.Format = texFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = bindFlags;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &texture);
    if (FAILED(hr))
        return;

    depthStencilViews.resize(arraySize);

    for (UINT i = 0; i < arraySize; ++i)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = dsvFormat;

        if (arraySize == 1)
        {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
        }
        else
        {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice = 0;
            dsvDesc.Texture2DArray.FirstArraySlice = i;
            dsvDesc.Texture2DArray.ArraySize = 1;
        }

        hr = device->CreateDepthStencilView(texture, &dsvDesc, &depthStencilViews[i]);
        if (FAILED(hr))
        {
            depthStencilViews[i] = nullptr;
        }
    }

    if (hasSRV)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = srvFormat;

        if (arraySize == 1)
        {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;
        }
        else
        {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MostDetailedMip = 0;
            srvDesc.Texture2DArray.MipLevels = 1;
            srvDesc.Texture2DArray.FirstArraySlice = 0;
            srvDesc.Texture2DArray.ArraySize = arraySize;
        }

        hr = device->CreateShaderResourceView(texture, &srvDesc, &srv);
        if (FAILED(hr))
        {
            srv = nullptr;
        }
    }
}

ID3D11DepthStencilView* DepthBufferD3D11::GetDSV(UINT arrayIndex) const
{
    if (arrayIndex >= depthStencilViews.size())
        return nullptr;

    return depthStencilViews[arrayIndex];
}   

ID3D11ShaderResourceView* DepthBufferD3D11::GetSRV() const
{
    return srv;
}

