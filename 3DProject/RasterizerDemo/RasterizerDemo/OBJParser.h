#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>
#include <d3d11_4.h>
#include <unordered_map>

struct OBJModel
{
	struct MeshVertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 texCoord;
	};
	std::vector<MeshVertex> vertices;
	std::vector<uint32_t> indices;

	struct ParsedMaterial
	{
		std::string mapKa;
		std::string mapKD;
		std::string mapKS;
		float shininess = 0.0f;
	};
	
	struct SubMeshInfo
	{
		size_t startIndex;
		size_t nrOfIndicesInSubMesh;
		ID3D11ShaderResourceView* ambientTextureSRV = nullptr;
		ID3D11ShaderResourceView* diffuseTextureSRV = nullptr;
		ID3D11ShaderResourceView* specularTextureSRV = nullptr;
		float shininess = 0.0f;
	};

	struct parseData
	{
		std::vector<DirectX::XMFLOAT3> positions;
		std::vector<DirectX::XMFLOAT2> uvs;
		std::vector<DirectX::XMFLOAT3> normals;

		std::unordered_map<std::string, ParsedMaterial> parsedMaterials;
		std::vector<MeshVertex> vertexData;
		std::vector<unsigned int> indexData;
		std::unordered_map<std::string, size_t> parsedFaces;

		std::string currentSubMeshMaterial;
		size_t currentSubMeshStartIndex = 0;

		std::vector<SubMeshInfo> finishedSubMeshes;
	};
	bool LoadFromFile(const std::string& filePath);
};