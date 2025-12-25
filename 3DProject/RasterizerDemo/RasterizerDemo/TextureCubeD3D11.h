#pragma once
#include <d3d11.h>

class TextureCubeD3D11
{
private:
    ID3D11Texture2D* m_textureCube = nullptr;
    ID3D11ShaderResourceView* m_srv = nullptr;
    ID3D11RenderTargetView* m_rtvs[6] = { nullptr }; // One RTV per cube face
    ID3D11DepthStencilView* m_dsv = nullptr;
    ID3D11Texture2D* m_depthTexture = nullptr;
    
    UINT m_width = 0;
    UINT m_height = 0;

public:
    TextureCubeD3D11() = default;
    ~TextureCubeD3D11();

// Prevent copying
    TextureCubeD3D11(const TextureCubeD3D11&) = delete;
    TextureCubeD3D11& operator=(const TextureCubeD3D11&) = delete;

    // Initialize the texture cube with specified resolution
    bool Initialize(ID3D11Device* device, UINT width, UINT height);

    // Getters for the views
    ID3D11ShaderResourceView* GetSRV() const { return m_srv; }
    ID3D11RenderTargetView* GetRTV(UINT faceIndex) const;
ID3D11DepthStencilView* GetDSV() const { return m_dsv; }
    
    // Get viewport for rendering to cube faces
    D3D11_VIEWPORT GetViewport() const;
    
    // Clear a specific face
    void ClearFace(ID3D11DeviceContext* context, UINT faceIndex, const float clearColor[4]);
    
    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
};
