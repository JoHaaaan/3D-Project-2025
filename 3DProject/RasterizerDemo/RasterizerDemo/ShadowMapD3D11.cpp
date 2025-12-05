#include "ShadowMapD3D11.h"

ShadowMapD3D11::~ShadowMapD3D11()
{
    if (m_srv) m_srv->Release();
    for (auto* dsv : m_dsvs) {
        if (dsv) dsv->Release();
    }
}

bool ShadowMapD3D11::Initialize(ID3D11Device* device, UINT width, UINT height, UINT arraySize)
{
    m_width = width;
    m_height = height;

    // 1. Create the Texture2DArray (Typeless to allow DSV and SRV)
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = arraySize;
    texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS; // Allow different views
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    ID3D11Texture2D* texture = nullptr;
    if (FAILED(device->CreateTexture2D(&texDesc, nullptr, &texture)))
        return false;

    // 2. Create the SRV (to read ALL slices in the Compute Shader)
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.ArraySize = arraySize;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.MostDetailedMip = 0;

    if (FAILED(device->CreateShaderResourceView(texture, &srvDesc, &m_srv)))
    {
        texture->Release();
        return false;
    }

    // 3. Create a DSV for EACH slice (to render into them individually)
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    dsvDesc.Texture2DArray.MipSlice = 0;
    dsvDesc.Texture2DArray.ArraySize = 1; // One slice at a time

    for (UINT i = 0; i < arraySize; ++i)
    {
        dsvDesc.Texture2DArray.FirstArraySlice = i;
        ID3D11DepthStencilView* dsv = nullptr;
        if (FAILED(device->CreateDepthStencilView(texture, &dsvDesc, &dsv)))
        {
            texture->Release();
            return false;
        }
        m_dsvs.push_back(dsv);
    }

    texture->Release();
    return true;
}

ID3D11DepthStencilView* ShadowMapD3D11::GetDSV(UINT sliceIndex) const
{
    if (sliceIndex < m_dsvs.size())
        return m_dsvs[sliceIndex];
    return nullptr;
}

D3D11_VIEWPORT ShadowMapD3D11::GetViewport() const
{
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)m_width;
    vp.Height = (float)m_height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    return vp;
}