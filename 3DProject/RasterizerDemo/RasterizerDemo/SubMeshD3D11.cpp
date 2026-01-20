#include "SubMeshD3D11.h"

SubMeshD3D11::~SubMeshD3D11()
{
	if (ambientTexture)
	{
		ambientTexture->Release();
		ambientTexture = nullptr;
	}
	if (diffuseTexture)
	{
		diffuseTexture->Release();
		diffuseTexture = nullptr;
	}
	if (specularTexture)
	{
		specularTexture->Release();
		specularTexture = nullptr;
	}
	if (normalHeightTexture)
	{
		normalHeightTexture->Release();
		normalHeightTexture = nullptr;
	}
}

SubMeshD3D11::SubMeshD3D11(SubMeshD3D11&& other) noexcept
	: startIndex(other.startIndex)
	, nrOfIndices(other.nrOfIndices)
	, ambientTexture(other.ambientTexture)
	, diffuseTexture(other.diffuseTexture)
	, specularTexture(other.specularTexture)
	, normalHeightTexture(other.normalHeightTexture)
{
	// Transfer ownership - nullify the source pointers
	other.ambientTexture = nullptr;
	other.diffuseTexture = nullptr;
	other.specularTexture = nullptr;
	other.normalHeightTexture = nullptr;
}

SubMeshD3D11& SubMeshD3D11::operator=(SubMeshD3D11&& other) noexcept
{
	if (this != &other)
	{
		// Release our current resources
		if (ambientTexture) ambientTexture->Release();
		if (diffuseTexture) diffuseTexture->Release();
		if (specularTexture) specularTexture->Release();
		if (normalHeightTexture) normalHeightTexture->Release();

		// Transfer ownership from other
		startIndex = other.startIndex;
		nrOfIndices = other.nrOfIndices;
		ambientTexture = other.ambientTexture;
		diffuseTexture = other.diffuseTexture;
		specularTexture = other.specularTexture;
		normalHeightTexture = other.normalHeightTexture;

		// Nullify source pointers
		other.ambientTexture = nullptr;
		other.diffuseTexture = nullptr;
		other.specularTexture = nullptr;
		other.normalHeightTexture = nullptr;
	}
	return *this;
}

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
