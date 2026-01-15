#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <DirectXMath.h>
#include <d3d11.h> // Needed for ID3D11Device
#include <unordered_map>

// Forward declarations
class MeshD3D11; // Use the class name directly
struct ID3D11ShaderResourceView;

// Definition of Vertex to match usage in ParseFace
struct Vertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 UV;
};

// Simple material info used by ParseData
struct MaterialInfo
{
    std::string name;
    DirectX::XMFLOAT3 ambient{ 0.2f, 0.2f, 0.2f };
    DirectX::XMFLOAT3 diffuse{ 0.8f, 0.8f, 0.8f };
    DirectX::XMFLOAT3 specular{ 0.5f, 0.5f, 0.5f };
    float specularPower{ 32.0f };

    std::string mapKa; // Ambient map
    std::string mapKd; // Diffuse map
    std::string mapKs; // Specular map
    std::string mapBump; // Normal/Height map for parallax occlusion mapping
};

// Info about one submesh inside a mesh
struct SubMeshInfo
{
    std::size_t startIndexValue = 0;
    std::size_t nrOfIndicesInSubMesh = 0;

    ID3D11ShaderResourceView* ambientTextureSRV = nullptr;
    ID3D11ShaderResourceView* diffuseTextureSRV = nullptr;
    ID3D11ShaderResourceView* specularTextureSRV = nullptr;
    ID3D11ShaderResourceView* normalHeightTextureSRV = nullptr; // For parallax occlusion mapping


    std::size_t materialIndex = 0;
    std::size_t currentSubMeshMaterial = 0;
};

// All temporary data used while parsing a single OBJ
struct ParseData
{
    // Geometry
    std::vector<DirectX::XMFLOAT3> positions;
    std::vector<DirectX::XMFLOAT3> normals;
    std::vector<DirectX::XMFLOAT2> texCoords;

    // Cache to avoid duplicating vertices (String identifier -> Index)
    std::unordered_map<std::string, unsigned int> vertexCache;

    // Final vertex buffer data
    std::vector<Vertex> vertices;   

    // Index data for the final mesh
    std::vector<unsigned int> indexData;

    // Materials and submeshes
    std::vector<MaterialInfo> parsedMaterials;
    std::vector<SubMeshInfo> finishedSubMeshes;

    std::size_t currentSubmeshStartIndex = 0;
    std::size_t currentSubMeshMaterial = 0;
};

// Global data used by the parser
extern std::string defaultDirectory;
// Note: Changed Mesh to MeshD3D11 to match your class name
extern std::unordered_map<std::string, MeshD3D11*> loadedMeshes; // Changed to pointers

struct TextureResource;
extern std::unordered_map<std::string, TextureResource> loadedTextures;

// Small helper functions to read tokens from a line
float GetLineFloat(const std::string& line, std::size_t& currentLinePos);
int   GetLineInt(const std::string& line, std::size_t& currentLinePos);
std::string GetLineString(const std::string& line, std::size_t& currentLinePos);

// Public entry point
// UPDATED: Now requires device to initialize the buffers
const MeshD3D11* GetMesh(const std::string& path, ID3D11Device* device);

// File reading
void ReadFile(const std::string& path, std::string& toFill);

// OBJ level parsing
// UPDATED: Now requires device
void ParseOBJ(const std::string& identifier, const std::string& contents, ID3D11Device* device);

// Per line parsing
void ParseLine(const std::string& line, ParseData& data);

// Handlers for specific identifiers
void ParsePosition(const std::string& dataSection, ParseData& data);
void ParseTexCoord(const std::string& dataSection, ParseData& data);
void ParseNormal(const std::string& dataSection, ParseData& data);
void ParseFace(const std::string& dataSection, ParseData& data);
void ParseMtlLib(const std::string& dataSection, ParseData& data);
void ParseUseMtl(const std::string& dataSection, ParseData& data);

// Helper to finish and store the current submesh
void PushBackCurrentSubmesh(ParseData& data);

void UnloadMeshes();

const MeshD3D11* GetMesh(const std::string& path, ID3D11Device* device);