#include "SubMeshD3D11.h"

void SubMeshD3D11::Initialize(size_t startIndexValue, size_t nrOfIndicesInSubMesh, ID3D11ShaderResourceView* ambientTextureSRV, ID3D11ShaderResourceView* diffuseTextureSRV, ID3D11ShaderResourceView* specularTextureSRV, ID3D11ShaderResourceView* normalHeightTextureSRV)
{
	startIndex = startIndexValue;
	nrOfIndices = nrOfIndicesInSubMesh;
	ambientTexture = ambientTextureSRV;
	diffuseTexture = diffuseTextureSRV;
	specularTexture = specularTextureSRV;
	normalHeightTexture = normalHeightTextureSRV;
}

void SubMeshD3D11::PerformDrawCall(ID3D11DeviceContext* context) const
{
	context->DrawIndexed(static_cast<UINT>(nrOfIndices), static_cast<UINT>(startIndex), 0);
}

ID3D11ShaderResourceView* SubMeshD3D11::GetAmbientSRV() const
{
	return ambientTexture;
}

ID3D11ShaderResourceView* SubMeshD3D11::GetDiffuseSRV() const
{
	return diffuseTexture;
}

ID3D11ShaderResourceView* SubMeshD3D11::GetSpecularSRV() const
{
	return specularTexture;
}

ID3D11ShaderResourceView* SubMeshD3D11::GetNormalHeightSRV() const
{
	return normalHeightTexture;
}
