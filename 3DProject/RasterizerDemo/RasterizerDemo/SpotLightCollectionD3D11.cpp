#include "SpotLightCollectionD3D11.h"

void SpotLightCollectionD3D11::Initialize(ID3D11Device* device, const SpotLightData& lightInfo)
{
}

void SpotLightCollectionD3D11::UpdateLightBuffers(ID3D11DeviceContext* context)
{
}

UINT SpotLightCollectionD3D11::GetNrOfLights() const
{
	return 0;
}

ID3D11DepthStencilView* SpotLightCollectionD3D11::GetShadowMapDSV(UINT lightIndex) const
{
	return nullptr;
}

ID3D11ShaderResourceView* SpotLightCollectionD3D11::GetShadowMapsSRV() const
{
	return nullptr;
}

ID3D11ShaderResourceView* SpotLightCollectionD3D11::GetLightBufferSRV() const
{
	return nullptr;
}

ID3D11Buffer* SpotLightCollectionD3D11::GetLightCameraConstantBuffer(UINT lightIndex) const
{
	return nullptr;
}
