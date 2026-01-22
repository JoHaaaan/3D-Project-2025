#pragma once
#include <d3d11.h>
#include <vector>

class ShadowMapD3D11
{

private:

    // SRV for sampling shadow depth during shading
    ID3D11ShaderResourceView* m_srv = nullptr;

    // One depth-stencil view per array slice
    std::vector<ID3D11DepthStencilView*> m_dsvs;
    UINT m_width = 0;
    UINT m_height = 0;


public:
    ShadowMapD3D11() = default;
    ~ShadowMapD3D11();

    // Creates a depth Texture2DArray with typeless format + SRV and one DSV per slice
    bool Initialize(ID3D11Device* device, UINT width, UINT height, UINT arraySize);

    // SRV for reading shadow depths in shaders
    ID3D11ShaderResourceView* GetSRV() const { return m_srv; }

    // Bind a specific slice to the Output Merger to write depth
    ID3D11DepthStencilView* GetDSV(UINT sliceIndex) const;

    // Viewport that matches the shadow map size (bind during shadow pass)
    D3D11_VIEWPORT GetViewport() const;
};