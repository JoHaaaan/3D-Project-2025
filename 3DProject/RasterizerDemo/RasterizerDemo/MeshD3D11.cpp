#include "MeshD3D11.h"
#include "PipelineHelper.h" // For SimpleVertex structure

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

  // Calculate local-space bounding box from vertex data
    if (meshInfo.vertexInfo.vertexData && meshInfo.vertexInfo.nrOfVerticesInBuffer > 0)
    {
        // Assume vertex format is SimpleVertex (pos[3], nrm[3], uv[2])
        const float* vertexData = static_cast<const float*>(meshInfo.vertexInfo.vertexData);
        size_t vertexStride = meshInfo.vertexInfo.sizeOfVertex / sizeof(float);

    DirectX::XMFLOAT3 minPos(FLT_MAX, FLT_MAX, FLT_MAX);
        DirectX::XMFLOAT3 maxPos(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (size_t i = 0; i < meshInfo.vertexInfo.nrOfVerticesInBuffer; ++i)
 {
            size_t offset = i * vertexStride;
            float x = vertexData[offset + 0];
       float y = vertexData[offset + 1];
 float z = vertexData[offset + 2];

        minPos.x = (std::min)(minPos.x, x);
            minPos.y = (std::min)(minPos.y, y);
        minPos.z = (std::min)(minPos.z, z);

            maxPos.x = (std::max)(maxPos.x, x);
 maxPos.y = (std::max)(maxPos.y, y);
        maxPos.z = (std::max)(maxPos.z, z);
        }

        // Calculate center and extents
        DirectX::XMFLOAT3 center(
            (minPos.x + maxPos.x) * 0.5f,
  (minPos.y + maxPos.y) * 0.5f,
            (minPos.z + maxPos.z) * 0.5f
        );

        DirectX::XMFLOAT3 extents(
   (maxPos.x - minPos.x) * 0.5f,
      (maxPos.y - minPos.y) * 0.5f,
            (maxPos.z - minPos.z) * 0.5f
        );

   localBoundingBox.Center = center;
        localBoundingBox.Extents = extents;
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