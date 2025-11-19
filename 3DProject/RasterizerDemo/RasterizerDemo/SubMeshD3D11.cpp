#include "SubMeshD3D11.h"

void SubMeshD3D11::Initialize(size_t startIndexValue, size_t nrOfIndicesInSubMesh, ID3D11ShaderResourceView* ambientTextureSRV, ID3D11ShaderResourceView* diffuseTextureSRV, ID3D11ShaderResourceView* specularTextureSRV)
{
}

void SubMeshD3D11::PerformDrawCall(ID3D11DeviceContext* context) const
{
}

ID3D11ShaderResourceView* SubMeshD3D11::GetAmbientSRV() const
{
	return nullptr;
}

ID3D11ShaderResourceView* SubMeshD3D11::GetDiffuseSRV() const
{
	return nullptr;
}

ID3D11ShaderResourceView* SubMeshD3D11::GetSpecularSRV() const
{
	return nullptr;
}
