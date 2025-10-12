#include "MeshD3D11.h"

void MeshD3D11::Initialize(ID3D11Device* device, const MeshData& meshInfo)
{
	// Initialize vertex buffer
	vertexBuffer.Initialize(
		device,
		static_cast<UINT>(meshInfo.vertexInfo.sizeOfVertex),
		static_cast<UINT>(meshInfo.vertexInfo.nrOfVerticesInBuffer),
		meshInfo.vertexInfo.vertexData
	);

	// Initialize index buffer
	indexBuffer.Initialize(
		device,
		meshInfo.indexInfo.nrOfIndicesInBuffer,
		meshInfo.indexInfo.indexData
	);

	// Initialize all sub-meshes
	subMeshes.clear();
	subMeshes.reserve(meshInfo.subMeshInfo.size());

	for (const auto& subMeshInfo : meshInfo.subMeshInfo)
	{
		SubMeshD3D11 subMesh;
		subMesh.Initialize(
			subMeshInfo.startIndexValue,
			subMeshInfo.nrOfIndicesInSubMesh,
			subMeshInfo.ambientTextureSRV,
			subMeshInfo.diffuseTextureSRV,
			subMeshInfo.specularTextureSRV
		);
		subMeshes.push_back(subMesh);
	}
}

void MeshD3D11::BindMeshBuffers(ID3D11DeviceContext* context) const
{
	// Bind vertex buffer
	UINT stride = vertexBuffer.GetVertexSize();
	UINT offset = 0;
	ID3D11Buffer* vb = vertexBuffer.GetBuffer();
	context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);

	// Bind index buffer
	ID3D11Buffer* ib = indexBuffer.GetBuffer();
	context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
}

void MeshD3D11::PerformSubMeshDrawCall(ID3D11DeviceContext* context, size_t subMeshIndex) const
{
	if (subMeshIndex < subMeshes.size())
	{
		subMeshes[subMeshIndex].PerformDrawCall(context);
	}
}

size_t MeshD3D11::GetNrOfSubMeshes() const
{
	return subMeshes.size();
}

ID3D11ShaderResourceView* MeshD3D11::GetAmbientSRV(size_t subMeshIndex) const
{
	if (subMeshIndex < subMeshes.size())
	{
		return subMeshes[subMeshIndex].GetAmbientSRV();
	}
	return nullptr;
}

ID3D11ShaderResourceView* MeshD3D11::GetDiffuseSRV(size_t subMeshIndex) const
{
	if (subMeshIndex < subMeshes.size())
	{
		return subMeshes[subMeshIndex].GetDiffuseSRV();
	}
	return nullptr;
}

ID3D11ShaderResourceView* MeshD3D11::GetSpecularSRV(size_t subMeshIndex) const
{
	if (subMeshIndex < subMeshes.size())
	{
		return subMeshes[subMeshIndex].GetSpecularSRV();
	}
	return nullptr;
}
