#pragma once

#include <d3d11_4.h>
#include "RenderTargetD3D11.h"

class GBufferD3D11
{
private:
    // Våra olika G-buffer render targets
    RenderTargetD3D11 albedoRT;  // färg
    RenderTargetD3D11 normalRT;  // normal
    RenderTargetD3D11 positionRT;    // Position

public:
    GBufferD3D11() = default;
    ~GBufferD3D11() = default; // RenderTargetD3D11 sköter sin egen release

    GBufferD3D11(const GBufferD3D11&) = delete;
    GBufferD3D11& operator=(const GBufferD3D11&) = delete;

    void Initialize(ID3D11Device* device, UINT width, UINT height);

    // Bind alla G-buffer RTVs + DSV inför geometry passet
    void SetAsRenderTargets(ID3D11DeviceContext* context,
        ID3D11DepthStencilView* dsv);

    // Lämpligt för debug / init: cleara alla RTVs
    void Clear(ID3D11DeviceContext* context,
        const float clearColor[4]);

    // SRVs till lighting-pass (compute shader senare)
    ID3D11ShaderResourceView* GetAlbedoSRV() const;
    ID3D11ShaderResourceView* GetNormalSRV() const;
    ID3D11ShaderResourceView* GetPositionSRV() const;

    // Om du vill kunna debug-kopiera dem direkt till backbuffer
    ID3D11RenderTargetView* GetAlbedoRTV() const { return albedoRT.GetRTV(); }
    ID3D11RenderTargetView* GetNormalRTV() const { return normalRT.GetRTV(); }
    ID3D11RenderTargetView* GetPositionRTV() const { return positionRT.GetRTV(); }
};
#pragma once
