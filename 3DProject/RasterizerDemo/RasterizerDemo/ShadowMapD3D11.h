#pragma once
#include <d3d11.h>
#include <vector>

class ShadowMapD3D11
{

private:
    ID3D11ShaderResourceView* m_srv = nullptr;
    std::vector<ID3D11DepthStencilView*> m_dsvs;
    UINT m_width = 0;
    UINT m_height = 0;


public:
    ShadowMapD3D11() = default;
    ~ShadowMapD3D11();

    // Create the Texture2DArray and Views
    bool Initialize(ID3D11Device* device, UINT width, UINT height, UINT arraySize);

    // Bind this SRV to the Compute Shader to read shadows
    ID3D11ShaderResourceView* GetSRV() const { return m_srv; }

    // Bind a specific slice to the Output Merger to write depth
    ID3D11DepthStencilView* GetDSV(UINT sliceIndex) const;

    // Helper to get a viewport matching the shadow map size
    D3D11_VIEWPORT GetViewport() const;
};