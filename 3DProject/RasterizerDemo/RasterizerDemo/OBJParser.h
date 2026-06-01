#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <DirectXMath.h>
#include <d3d11.h>

class MeshD3D11;
struct ID3D11ShaderResourceView;

struct Vertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 UV;
};

struct MaterialInfo
{
	std::string name;
	DirectX::XMFLOAT3 ambient{ 0.2f, 0.2f, 0.2f };
	DirectX::XMFLOAT3 diffuse{ 0.8f, 0.8f, 0.8f };
	DirectX::XMFLOAT3 specular{ 0.5f, 0.5f, 0.5f };
	float specularPower{ 32.0f };

	std::string mapKa;
	std::string mapKd;
	std::string mapKs;
	std::string mapBump;
};

struct SubMeshInfo
{
	std::size_t startIndexValue = 0;
	std::size_t nrOfIndicesInSubMesh = 0;

	ID3D11ShaderResourceView* ambientTextureSRV = nullptr;
	ID3D11ShaderResourceView* diffuseTextureSRV = nullptr;
	ID3D11ShaderResourceView* specularTextureSRV = nullptr;
	ID3D11ShaderResourceView* normalHeightTextureSRV = nullptr;

	std::size_t materialIndex = 0;
	std::size_t currentSubMeshMaterial = 0;
};

struct ParseData
{
	std::vector<DirectX::XMFLOAT3> positions;
	std::vector<DirectX::XMFLOAT3> normals;
	std::vector<DirectX::XMFLOAT2> texCoords;

	std::unordered_map<std::string, unsigned int> vertexCache;

	std::vector<Vertex> vertices;
	std::vector<unsigned int> indexData;

	std::vector<MaterialInfo> parsedMaterials;
	std::vector<SubMeshInfo> finishedSubMeshes;

	std::size_t currentSubmeshStartIndex = 0;
	std::size_t currentSubMeshMaterial = 0;
};

extern std::string defaultDirectory;
extern std::unordered_map<std::string, MeshD3D11*> loadedMeshes;



float GetLineFloat(const std::string& line, std::size_t& currentLinePos);
int GetLineInt(const std::string& line, std::size_t& currentLinePos);
std::string GetLineString(const std::string& line, std::size_t& currentLinePos);

const MeshD3D11* GetMesh(const std::string& path, ID3D11Device* device);

void ReadFile(const std::string& path, std::string& toFill);

void ParseOBJ(const std::string& identifier, const std::string& contents, ID3D11Device* device);

void ParseLine(const std::string& line, ParseData& data);

void ParsePosition(const std::string& dataSection, ParseData& data);
void ParseTexCoord(const std::string& dataSection, ParseData& data);
void ParseNormal(const std::string& dataSection, ParseData& data);
void ParseFace(const std::string& dataSection, ParseData& data);
void ParseMtlLib(const std::string& dataSection, ParseData& data);
void ParseUseMtl(const std::string& dataSection, ParseData& data);

void PushBackCurrentSubmesh(ParseData& data);

void UnloadMeshes();