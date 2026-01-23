#include "DepthBufferD3D11.h"


// DEPTH BUFFER
// Supports standard depth testing, shadow mapping (SRV), and texture arrays
// Format: 24-bit depth + 8-bit stencil (D24_UNORM_S8_UINT)
// Typeless format (R24G8_TYPELESS) enables both DSV and SRV creation for shadow mapping

DepthBufferD3D11::DepthBufferD3D11(ID3D11Device* device, UINT width, UINT height, bool hasSRV)
{
    // Single depth buffer with optional SRV for shadow mapping
	Initialize(device, width, height, hasSRV, 1);
}

DepthBufferD3D11::~DepthBufferD3D11()
{
    // Release all array slice DSVs
    for (size_t i = 0; i < depthStencilViews.size(); ++i)
    {
        if (depthStencilViews[i])
        {
            depthStencilViews[i]->Release();
            depthStencilViews[i] = nullptr;
        }
    }
    depthStencilViews.clear();
    
    // Optional SRV used for sampling depth in shaders (shadow mapping/post-process)
    if (srv)
    {
        srv->Release();
        srv = nullptr;
    }

	// Release underlying texture reasource
    if (texture)
    {
        texture->Release();
        texture = nullptr;
    }
}


void DepthBufferD3D11::Initialize(ID3D11Device* device, UINT width, UINT height, bool hasSRV, UINT arraySize)
{
    // Clear existing resources before re-initialization
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

    // Ensure at least one slice
    if (arraySize == 0)
        arraySize = 1;
    
    // Default formats for depth-only usage
    DXGI_FORMAT texFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DXGI_FORMAT dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;

    UINT bindFlags = D3D11_BIND_DEPTH_STENCIL;

    if (hasSRV)
    {
        // Create the texture as TYPELESS so it can be viewed both as a DSV and an SRV.
		// Needed for shadow mapping.
        texFormat = DXGI_FORMAT_R24G8_TYPELESS;
        dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        bindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    
    // Create depth texture (Texture2D or Texture2DArray)
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
    
    // Create one DSV per slice so each array index can be bound as depth target
    depthStencilViews.resize(arraySize);

    for (UINT i = 0; i < arraySize; ++i)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = dsvFormat;

        if (arraySize == 1)
        {
            // Normal 2D depth buffer
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
        }
        else
        {
            // Depth texture array slice (one slice per DSV)
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
        // Create SRV for sampling the depth buffer in shaders.
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
            // SRV covers entire array for sampling in shaders
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
    // Returns a DSV for a specific array slice
    if (arrayIndex >= depthStencilViews.size())
        return nullptr;

    return depthStencilViews[arrayIndex];
}   

ID3D11ShaderResourceView* DepthBufferD3D11::GetSRV() const
{
    return srv;
}

