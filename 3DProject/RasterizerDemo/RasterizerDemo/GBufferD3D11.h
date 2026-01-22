#pragma once

#include <d3d11_4.h>
#include "RenderTargetD3D11.h"

class GBufferD3D11
{
private:
    RenderTargetD3D11 albedoRT;
    RenderTargetD3D11 normalRT;
    RenderTargetD3D11 positionRT;

public:
    GBufferD3D11() = default;
    ~GBufferD3D11() = default;

    GBufferD3D11(const GBufferD3D11&) = delete;
    GBufferD3D11& operator=(const GBufferD3D11&) = delete;

    void Initialize(ID3D11Device* device, UINT width, UINT height);

    void SetAsRenderTargets(ID3D11DeviceContext* context, ID3D11DepthStencilView* dsv);

    void Clear(ID3D11DeviceContext* context, const float clearColor[4]);

    ID3D11ShaderResourceView* GetAlbedoSRV() const;
    ID3D11ShaderResourceView* GetNormalSRV() const;
    ID3D11ShaderResourceView* GetPositionSRV() const;

    ID3D11RenderTargetView* GetAlbedoRTV() const { return albedoRT.GetRTV(); }
    ID3D11RenderTargetView* GetNormalRTV() const { return normalRT.GetRTV(); }
    ID3D11RenderTargetView* GetPositionRTV() const { return positionRT.GetRTV(); }
};
