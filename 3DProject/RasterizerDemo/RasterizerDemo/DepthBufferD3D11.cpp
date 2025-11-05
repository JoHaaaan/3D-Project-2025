#include "DepthBufferD3D11.h"

DepthBufferD3D11::DepthBufferD3D11(ID3D11Device* device, UINT width, UINT height, bool hasSRV)
{
}

DepthBufferD3D11::~DepthBufferD3D11()
{
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
