#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <fstream>

#include <DirectXMath.h>

// Forward declarations
struct Mesh;
struct ID3D11ShaderResourceView;

// Simple material info used by ParseData
struct MaterialInfo
{
    std::string mapKa; // Ambient map
    std::string mapKd; // Diffuse map
    std::string mapKs; // Specular map
};

// Info about one submesh inside a mesh
struct SubMeshInfo
{
    std::size_t startIndexValue = 0;
    std::size_t nrOfIndicesInSubMesh = 0;

    ID3D11ShaderResourceView* ambientTextureSRV = nullptr;
    ID3D11ShaderResourceView* diffuseTextureSRV = nullptr;
    ID3D11ShaderResourceView* specularTextureSRV = nullptr;
};

// All temporary data used while parsing a single OBJ
struct ParseData
{
    // Geometry
    std::vector<DirectX::XMFLOAT3> positions;
    std::vector<DirectX::XMFLOAT3> normals;
    std::vector<DirectX::XMFLOAT2> texCoords;

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
extern std::unordered_map<std::string, Mesh> loadedMeshes;

// If you have a texture manager somewhere you can expose it here
// Just forward declare the type and map
struct TextureResource;
extern std::unordered_map<std::string, TextureResource> loadedTextures;

// Small helper functions to read tokens from a line
float GetLineFloat(const std::string& line, std::size_t& currentLinePos);
int   GetLineInt(const std::string& line, std::size_t& currentLinePos);
std::string GetLineString(const std::string& line, std::size_t& currentLinePos);

// Public entry point
// Returns a pointer to a cached mesh
const Mesh* GetMesh(const std::string& path);

// File reading
void ReadFile(const std::string& path, std::string& toFill);

// OBJ level parsing
void ParseOBJ(const std::string& identifier, const std::string& contents);

// Per line parsing
void ParseLine(const std::string& line, ParseData& data);

// Handlers for specific identifiers
void ParsePosition(const std::string& dataSection, ParseData& data);

// Helper to finish and store the current submesh
void PushBackCurrentSubmesh(ParseData& data);
