#include "MeshD3D11.h"

void MeshD3D11::Initialize(ID3D11Device* device, const MeshData& meshInfo)
{
}

void MeshD3D11::BindMeshBuffers(ID3D11DeviceContext* context) const
{
}

void MeshD3D11::PerformSubMeshDrawCall(ID3D11DeviceContext* context, size_t subMeshIndex) const
{
}

size_t MeshD3D11::GetNrOfSubMeshes() const
{
	return size_t();
}

ID3D11ShaderResourceView* MeshD3D11::GetAmbientSRV(size_t subMeshIndex) const
{
	return nullptr;
}

ID3D11ShaderResourceView* MeshD3D11::GetDiffuseSRV(size_t subMeshIndex) const
{
	return nullptr;
}

ID3D11ShaderResourceView* MeshD3D11::GetSpecularSRV(size_t subMeshIndex) const
{
	return nullptr;
}
