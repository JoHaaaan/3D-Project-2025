#pragma once

#include <vector>

#include <d3d11_4.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>

#include "SubMeshD3D11.h"
#include "VertexBufferD3D11.h"
#include "IndexBufferD3D11.h"

struct MeshData
{
	struct MaterialData
	{
		DirectX::XMFLOAT3 ambient;
		DirectX::XMFLOAT3 diffuse;
		DirectX::XMFLOAT3 specular;
		float specularPower;
	};

	struct VertexInfo
	{
		size_t sizeOfVertex;
		size_t nrOfVerticesInBuffer;
		void* vertexData;
	}vertexInfo;
	struct IndexInfo
	{
		size_t nrOfIndicesInBuffer;
		uint32_t* indexData;
	} indexInfo;

	struct SubMeshInfo
	{
		size_t startIndexValue;
		size_t nrOfIndicesInSubMesh;
		ID3D11ShaderResourceView* ambientTextureSRV;
		ID3D11ShaderResourceView* diffuseTextureSRV;
		ID3D11ShaderResourceView* specularTextureSRV;
		ID3D11ShaderResourceView* normalHeightTextureSRV; // For parallax occlusion mapping
		MaterialData material;
		size_t materialIndex;
	};

	std::vector<SubMeshInfo> subMeshInfo;
};

class MeshD3D11
{
private:
	std::vector<SubMeshD3D11> subMeshes;
	std::vector<MeshData::MaterialData> subMeshMaterials;
	VertexBufferD3D11 vertexBuffer;
	IndexBufferD3D11 indexBuffer;
	DirectX::BoundingBox localBoundingBox;  // Local-space bounding box

public:
	MeshD3D11() = default;
	~MeshD3D11() = default;
	MeshD3D11(const MeshD3D11& other) = delete;
	MeshD3D11& operator=(const MeshD3D11& other) = delete;
	MeshD3D11(MeshD3D11&& other) = delete;
	MeshD3D11& operator=(MeshD3D11&& other) = delete;

	void Initialize(ID3D11Device* device, const MeshData& meshInfo);

	void BindMeshBuffers(ID3D11DeviceContext* context) const;
	void PerformSubMeshDrawCall(ID3D11DeviceContext* context, size_t subMeshIndex) const;

	size_t GetNrOfSubMeshes() const;
	ID3D11ShaderResourceView* GetAmbientSRV(size_t subMeshIndex) const;
	ID3D11ShaderResourceView* GetDiffuseSRV(size_t subMeshIndex) const;
	ID3D11ShaderResourceView* GetSpecularSRV(size_t subMeshIndex) const;
	ID3D11ShaderResourceView* GetNormalHeightSRV(size_t subMeshIndex) const;
	const MeshData::MaterialData& GetMaterial(size_t subMeshIndex) const;

	// Get the local-space bounding box
	const DirectX::BoundingBox& GetLocalBoundingBox() const { return localBoundingBox; }
};