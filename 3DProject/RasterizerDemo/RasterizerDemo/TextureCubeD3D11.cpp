#include "TextureCubeD3D11.h"
#include <Windows.h>
#include <stdexcept>

TextureCubeD3D11::~TextureCubeD3D11()
{
    if (m_srv) m_srv->Release();
    if (m_textureCube) m_textureCube->Release();
    if (m_dsv) m_dsv->Release();
    if (m_depthTexture) m_depthTexture->Release();
    
    for (int i = 0; i < 6; ++i)
    {
        if (m_rtvs[i]) m_rtvs[i]->Release();
    }
}

bool TextureCubeD3D11::Initialize(ID3D11Device* device, UINT width, UINT height, bool needsSRV)
{
    m_width = width;
    m_height = height;

    // Creating the textures
    D3D11_TEXTURE2D_DESC desc;
  ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 6;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    desc.BindFlags |= needsSRV ? D3D11_BIND_SHADER_RESOURCE : 0;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &m_textureCube);

    if (FAILED(hr))
    {
        throw std::runtime_error("Could not create texture cube");
    }

    // Creating the required resource views for the texture cube
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
 rtvDesc.Format = desc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    rtvDesc.Texture2DArray.ArraySize = 1;
    rtvDesc.Texture2DArray.MipSlice = 0;

    for (int i = 0; i < 6; ++i)
    {
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        hr = device->CreateRenderTargetView(m_textureCube, &rtvDesc, &m_rtvs[i]);

        if (FAILED(hr))
        {
          throw std::runtime_error("Could not create texture cube rtv");
        }
    }

    if (needsSRV == true)
    {
        hr = device->CreateShaderResourceView(m_textureCube, nullptr, &m_srv);

  if (FAILED(hr))
        {
            throw std::runtime_error("Could not create texture cube srv");
        }
    }

    // Create shared depth buffer for all cube faces
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
  depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthDesc.CPUAccessFlags = 0;
    depthDesc.MiscFlags = 0;

    hr = device->CreateTexture2D(&depthDesc, nullptr, &m_depthTexture);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create depth buffer for texture cube!\n");
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = depthDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = device->CreateDepthStencilView(m_depthTexture, &dsvDesc, &m_dsv);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create DSV for texture cube!\n");
    return false;
    }

    OutputDebugStringA("Texture cube initialized successfully!\n");
    return true;
}

ID3D11RenderTargetView* TextureCubeD3D11::GetRTV(UINT faceIndex) const
{
    if (faceIndex >= 6)
        return nullptr;
    return m_rtvs[faceIndex];
}

D3D11_VIEWPORT TextureCubeD3D11::GetViewport() const
{
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    return viewport;
}

void TextureCubeD3D11::ClearFace(ID3D11DeviceContext* context, UINT faceIndex, const float clearColor[4])
{
    if (faceIndex >= 6 || !m_rtvs[faceIndex])
    return;
    
    context->ClearRenderTargetView(m_rtvs[faceIndex], clearColor);
    context->ClearDepthStencilView(m_dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}
