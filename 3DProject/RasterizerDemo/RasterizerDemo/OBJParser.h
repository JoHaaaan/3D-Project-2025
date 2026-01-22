#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <DirectXMath.h>
#include <d3d11.h>

// Forward declarations
class MeshD3D11;
struct ID3D11ShaderResourceView;

// Vertex structure matching the OBJ file format (position, normal, UV)
struct Vertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 UV;
};

// Material properties parsed from MTL files
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

// Submesh data within a mesh (material group)
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

// Intermediate data structure used during OBJ parsing
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

// Global mesh cache and default directory for OBJ files
extern std::string defaultDirectory;
extern std::unordered_map<std::string, MeshD3D11*> loadedMeshes;

struct TextureResource;
extern std::unordered_map<std::string, TextureResource> loadedTextures;

// Token parsing utilities
float GetLineFloat(const std::string& line, std::size_t& currentLinePos);
int GetLineInt(const std::string& line, std::size_t& currentLinePos);
std::string GetLineString(const std::string& line, std::size_t& currentLinePos);

// Retrieves or loads a mesh from cache
const MeshD3D11* GetMesh(const std::string& path, ID3D11Device* device);

// File I/O
void ReadFile(const std::string& path, std::string& toFill);

// OBJ parsing entry point
void ParseOBJ(const std::string& identifier, const std::string& contents, ID3D11Device* device);

// Line-by-line parsing dispatcher
void ParseLine(const std::string& line, ParseData& data);

// OBJ element parsers
void ParsePosition(const std::string& dataSection, ParseData& data);
void ParseTexCoord(const std::string& dataSection, ParseData& data);
void ParseNormal(const std::string& dataSection, ParseData& data);
void ParseFace(const std::string& dataSection, ParseData& data);
void ParseMtlLib(const std::string& dataSection, ParseData& data);
void ParseUseMtl(const std::string& dataSection, ParseData& data);

// Finalizes the current submesh and adds it to the list
void PushBackCurrentSubmesh(ParseData& data);

// Cleanup for cached meshes
void UnloadMeshes();