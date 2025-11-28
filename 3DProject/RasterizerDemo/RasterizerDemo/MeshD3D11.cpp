#include "MeshD3D11.h"

void MeshD3D11::Initialize(ID3D11Device* device, const MeshData& meshInfo)
{
    vertexBuffer.Initialize(
        device,
        static_cast<UINT>(meshInfo.vertexInfo.sizeOfVertex),
        static_cast<UINT>(meshInfo.vertexInfo.nrOfVerticesInBuffer),
        meshInfo.vertexInfo.vertexData
    );

    indexBuffer.Initialize(
        device,
        meshInfo.indexInfo.nrOfIndicesInBuffer,
        meshInfo.indexInfo.indexData
    );

    subMeshes.clear();
    subMeshes.reserve(meshInfo.subMeshInfo.size());
    subMeshMaterials.clear();
    subMeshMaterials.reserve(meshInfo.subMeshInfo.size());

    for (const auto& sm : meshInfo.subMeshInfo)
    {
        SubMeshD3D11 subMesh;
        subMesh.Initialize(
            sm.startIndexValue,
            sm.nrOfIndicesInSubMesh,
            sm.ambientTextureSRV,
            sm.diffuseTextureSRV,
            sm.specularTextureSRV
        );

        subMeshes.push_back(subMesh);
        subMeshMaterials.push_back(sm.material);

    }
}

void MeshD3D11::BindMeshBuffers(ID3D11DeviceContext* context) const
{
    UINT stride = vertexBuffer.GetVertexSize();
    UINT offset = 0;

    ID3D11Buffer* vb = vertexBuffer.GetBuffer();
    context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);

    context->IASetIndexBuffer(indexBuffer.GetBuffer(), DXGI_FORMAT_R32_UINT, 0);
}

void MeshD3D11::PerformSubMeshDrawCall(ID3D11DeviceContext* context, size_t subMeshIndex) const
{
	subMeshes[subMeshIndex].PerformDrawCall(context);
}

size_t MeshD3D11::GetNrOfSubMeshes() const
{
	return subMeshes.size();
}

ID3D11ShaderResourceView* MeshD3D11::GetAmbientSRV(size_t subMeshIndex) const
{
	return subMeshes[subMeshIndex].GetAmbientSRV();
}

ID3D11ShaderResourceView* MeshD3D11::GetDiffuseSRV(size_t subMeshIndex) const
{
	return subMeshes[subMeshIndex].GetDiffuseSRV();
}

ID3D11ShaderResourceView* MeshD3D11::GetSpecularSRV(size_t subMeshIndex) const
{
    return subMeshes[subMeshIndex].GetSpecularSRV();
}


const MeshData::MaterialData& MeshD3D11::GetMaterial(size_t subMeshIndex) const
{
    return subMeshMaterials[subMeshIndex];
}