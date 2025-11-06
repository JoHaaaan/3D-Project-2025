#include "DepthBufferD3D11.h"

DepthBufferD3D11::DepthBufferD3D11(ID3D11Device* device, UINT width, UINT height, bool hasSRV)
{
	Initialize(device, width, height, hasSRV, 1);
}

DepthBufferD3D11::~DepthBufferD3D11()
{
    // Släpp alla DepthStencilViews först (views håller referenser till texturen)
    for (size_t i = 0; i < depthStencilViews.size(); ++i)
    {
        if (depthStencilViews[i])
        {
            depthStencilViews[i]->Release();
            depthStencilViews[i] = nullptr;
        }
    }
    depthStencilViews.clear();

    // Släpp SRV efter DSV:erna
    if (srv)
    {
        srv->Release();
        srv = nullptr;
    }

    // Släpp den underliggande texturen sist
    if (texture)
    {
        texture->Release();
        texture = nullptr;
    }
}

void DepthBufferD3D11::Initialize(ID3D11Device* device, UINT width, UINT height, bool hasSRV, UINT arraySize)
{

}

ID3D11DepthStencilView* DepthBufferD3D11::GetDSV(UINT arrayIndex) const
{
	return nullptr;
}

ID3D11ShaderResourceView* DepthBufferD3D11::GetSRV() const
{
	return nullptr;
}
