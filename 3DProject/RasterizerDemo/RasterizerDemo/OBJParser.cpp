#include "OBJParser.h"
#include "MeshD3D11.h"

// Initialize global maps
std::string defaultDirectory = ""; // Adjust if needed
std::unordered_map<std::string, MeshD3D11*> loadedMeshes; // Changed to pointers

namespace
{
    std::string Trim(const std::string& str)
    {
        const auto start = str.find_first_not_of(" \t\r\n");
        const auto end = str.find_last_not_of(" \t\r\n");
        if (start == std::string::npos || end == std::string::npos)
        {
            return "";
        }
        return str.substr(start, end - start + 1);
    }
}

void UnloadMeshes()
{
    for (auto& pair : loadedMeshes)
    {
        if (pair.second)
        {
            delete pair.second; // Invokes MeshD3D11 destructor -> releases buffers
            pair.second = nullptr;
        }
    }
    loadedMeshes.clear();
}

float GetLineFloat(const std::string& line, size_t& currentLinePos)
{
    // Skip leading whitespace
    while (currentLinePos < line.size() && line[currentLinePos] == ' ')
    {
        currentLinePos++;
    }
    
    size_t numberStart = currentLinePos;

	while (currentLinePos < line.size() && line[currentLinePos] != ' ')
	{
		currentLinePos++;
	}

	std::string part = line.substr(numberStart, currentLinePos - numberStart);
	float extractedAndConvertedFloat = std::stof(part);

    return extractedAndConvertedFloat;
}

int GetLineInt(const std::string& line, size_t& currentLinePos)
{
    size_t numberStart = currentLinePos;

	while (currentLinePos < line.size() && line[currentLinePos] != ' ' && line[currentLinePos] != '/')
	{
		currentLinePos++;
	}

	std::string part = line.substr(numberStart, currentLinePos - numberStart);
	int extractedAndConvertedInteger = std::stoi(part);

	return extractedAndConvertedInteger;
}

std::string GetLineString(const std::string& line, size_t& currentLinePos)
{
	size_t numberStart = currentLinePos;

	while (currentLinePos < line.size() && line[currentLinePos] != ' ')
	{
		currentLinePos++;
	}
	std::string extractedString = line.substr(numberStart, currentLinePos - numberStart);

	return extractedString;
}

const MeshD3D11* GetMesh(const std::string& path, ID3D11Device* device) {
	// loadedMeshes is our persistent map of meshes we have parsed

	if (loadedMeshes.find(path) == loadedMeshes.end())
	{
		std::string fileData;
		ReadFile(path, fileData);
		ParseOBJ(path, fileData, device);
	}

	return loadedMeshes[path];
}

void ReadFile(const std::string& path, std::string& toFill)
{
	std::ifstream reader;
	reader.open(defaultDirectory + path);
	if (!reader.is_open())
	{
		throw std::runtime_error("Failed to open file: " + path);
	}

	reader.seekg(0, std::ios::end);
	toFill.reserve(static_cast<unsigned int>(reader.tellg()));
	reader.seekg(0, std::ios::beg);

	toFill.assign((std::istreambuf_iterator<char>(reader)),
		std::istreambuf_iterator<char>());
}

void ParseOBJ(const std::string& identifier, const std::string& contents, ID3D11Device* device)
{
    std::istringstream lineStream(contents);
    ParseData data;

    // Default material in case the OBJ doesn't reference one
    data.parsedMaterials.push_back(MaterialInfo{});


    std::string line;
    while (std::getline(lineStream, line))
    {
        ParseLine(line, data);
    }
    PushBackCurrentSubmesh(data);

    // --- FIX STARTS HERE ---
    // 1. Create a MeshData struct to transfer data to the MeshD3D11
    MeshData meshInfo = {};

    // 2. Fill Vertex Info
    meshInfo.vertexInfo.sizeOfVertex = sizeof(Vertex);
    meshInfo.vertexInfo.nrOfVerticesInBuffer = data.vertices.size();
    meshInfo.vertexInfo.vertexData = data.vertices.data();

    // 3. Fill Index Info
    meshInfo.indexInfo.nrOfIndicesInBuffer = data.indexData.size();
    meshInfo.indexInfo.indexData = data.indexData.data();

    // 4. Fill SubMesh Info
    for (const auto& sub : data.finishedSubMeshes)
    {
        MeshData::SubMeshInfo sm = {};
        sm.startIndexValue = sub.startIndexValue;
        sm.nrOfIndicesInSubMesh = sub.nrOfIndicesInSubMesh;
        sm.ambientTextureSRV = nullptr;  // No texture loading for now
        sm.diffuseTextureSRV = nullptr;
        sm.specularTextureSRV = nullptr;
        sm.materialIndex = sub.currentSubMeshMaterial;

        const auto& material = data.parsedMaterials[sm.materialIndex];
        sm.material.ambient = material.ambient;
        sm.material.diffuse = material.diffuse;
        sm.material.specular = material.specular;
        sm.material.specularPower = material.specularPower;
        meshInfo.subMeshInfo.push_back(sm);
    }

    // 5. Initialize the mesh - use new to create on heap since move is deleted
    MeshD3D11* toAdd = new MeshD3D11();
    toAdd->Initialize(device, meshInfo);
    // --- FIX ENDS HERE ---

    loadedMeshes[identifier] = toAdd;
}

void ParseLine(const std::string& line, ParseData& data)
{
	if (line.empty() || line[0] == '#') return;

	size_t pos = 0;
	std::string type = GetLineString(line, pos);

	if (type == "v")       ParsePosition(line.substr(pos), data);
	else if (type == "vt") ParseTexCoord(line.substr(pos), data);
	else if (type == "vn") ParseNormal(line.substr(pos), data);
	else if (type == "f")  ParseFace(line.substr(pos), data);
	else if (type == "mtllib") ParseMtlLib(line.substr(pos), data);
	else if (type == "usemtl") ParseUseMtl(line.substr(pos), data);
}

void ParsePosition(const std::string& dataSection, ParseData& data)
{
	size_t currentPos = 0; // Assuming string starts at the data section's first character

	DirectX::XMFLOAT3 toAdd;
	toAdd.x = GetLineFloat(dataSection, currentPos);
	++currentPos;
	toAdd.y = GetLineFloat(dataSection, currentPos);
	++currentPos;
	toAdd.z = GetLineFloat(dataSection, currentPos);

	data.positions.push_back(toAdd);
}

void ParseTexCoord(const std::string& dataSection, ParseData& data)
{
    size_t pos = 0;
    float u = GetLineFloat(dataSection, pos);
    ++pos; // skip space
    float v = GetLineFloat(dataSection, pos);
    data.texCoords.push_back({ u, 1.0f - v }); // Invert V for DirectX
}

void ParseNormal(const std::string& dataSection, ParseData& data)
{
    size_t pos = 0;
    float x = GetLineFloat(dataSection, pos);
    ++pos; // skip space
    float y = GetLineFloat(dataSection, pos);
    ++pos; // skip space
    float z = GetLineFloat(dataSection, pos);
    data.normals.push_back({ x, y, -z }); // Invert Z for left-handed coordinate system
}
struct VertexData
{
    int vInd;
    int tInd;
    int nInd;
};
VertexData ParseFaceVertex(const std::string& dataSection, size_t& pos)
{
    VertexData vertex = { 0, 0, 0 };

    vertex.vInd = GetLineInt(dataSection, pos);
    vertex.tInd = 0;
    vertex.nInd = 0;

    // Check for texture coordinate
    if (pos < dataSection.size() && dataSection[pos] == '/')
    {
        pos++; // skip first slash
        if (pos < dataSection.size() && dataSection[pos] != '/')
        {
            vertex.tInd = GetLineInt(dataSection, pos);
        }

        // Check for normal
        if (pos < dataSection.size() && dataSection[pos] == '/')
        {
            pos++; // skip second slash
            vertex.nInd = GetLineInt(dataSection, pos);
        }
    }

    return vertex;
}

void ParseFace(const std::string & dataSection, ParseData & data)
{
    size_t pos = 0;
    std::vector<VertexData> faceVertices;

    // Parse all vertices in the face
    while (pos < dataSection.size() && dataSection[pos] != '\0')
    {
        // Skip leading whitespace
        while (pos < dataSection.size() && dataSection[pos] == ' ')
            pos++;

        if (pos >= dataSection.size())
            break;

        VertexData vertex = ParseFaceVertex(dataSection, pos);
        faceVertices.push_back(vertex);
    }

    // OBJ faces can be triangles or quads; triangulate if needed
    if (faceVertices.size() >= 3)
    {
        // Fan triangulation for quads and n-gons
        for (size_t i = 1; i < faceVertices.size() - 1; ++i)
        {
            std::vector<VertexData> triangle = {
                faceVertices[0],
                faceVertices[i],
                faceVertices[i + 1]
            };

            for (const auto& vdata : triangle)
            {
                // Create cache key (e.g., "1/1/1")
                std::string token = std::to_string(vdata.vInd) + "/" +
                    std::to_string(vdata.tInd) + "/" +
                    std::to_string(vdata.nInd);

                // Check cache
                if (data.vertexCache.find(token) != data.vertexCache.end())
                {
                    data.indexData.push_back(data.vertexCache[token]);
                    continue;
                }

                // Create new vertex
                Vertex newVert = {};

                // Handle negative indices (relative) and 1-based indices
                int vInd = vdata.vInd;
                int tInd = vdata.tInd;
                int nInd = vdata.nInd;

                if (vInd < 0) vInd = (int)data.positions.size() + vInd + 1;
                if (tInd < 0) tInd = (int)data.texCoords.size() + tInd + 1;
                if (nInd < 0) nInd = (int)data.normals.size() + nInd + 1;

                // Assign vertex data (1-based indexing in OBJ)
                if (vInd > 0 && vInd <= (int)data.positions.size())
                    newVert.Position = data.positions[vInd - 1];

                if (tInd > 0 && tInd <= (int)data.texCoords.size())
                    newVert.UV = data.texCoords[tInd - 1];

                if (nInd > 0 && nInd <= (int)data.normals.size())
                    newVert.Normal = data.normals[nInd - 1];

                // Add to buffer and cache
                unsigned int newIndex = (unsigned int)data.vertices.size();
                data.vertices.push_back(newVert);
                data.indexData.push_back(newIndex);
                data.vertexCache[token] = newIndex;
            }
        }
    }
}

void ParseMtlLib(const std::string& dataSection, ParseData& data)
{
    const std::string mtlPath = Trim(dataSection);
    if (mtlPath.empty())
    {
        return;
    }

    std::string fileContents;
    ReadFile(mtlPath, fileContents);

    std::istringstream mtlStream(fileContents);
    std::string line;
    MaterialInfo* current = nullptr;

    while (std::getline(mtlStream, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream lineStream(line);
        std::string type;
        lineStream >> type;

        if (type == "newmtl")
        {
            std::string name;
            std::getline(lineStream, name);
            name = Trim(name);
            data.parsedMaterials.push_back(MaterialInfo{});
            current = &data.parsedMaterials.back();
            current->name = name;
        }
        else if (current && type == "Ka")
        {
            lineStream >> current->ambient.x >> current->ambient.y >> current->ambient.z;
        }
        else if (current && type == "Kd")
        {
            lineStream >> current->diffuse.x >> current->diffuse.y >> current->diffuse.z;
        }
        else if (current && type == "Ks")
        {
            lineStream >> current->specular.x >> current->specular.y >> current->specular.z;
        }
        else if (current && type == "Ns")
        {
            lineStream >> current->specularPower;
        }
        else if (current && type == "map_Kd")
        {
            std::getline(lineStream, current->mapKd);
            current->mapKd = Trim(current->mapKd);
        }
    }
}

void ParseUseMtl(const std::string& dataSection, ParseData& data)
{
    size_t pos = 0;
    std::string mtlName = GetLineString(dataSection, pos);

    // If we have indices, push the previous submesh
    if (data.indexData.size() > data.currentSubmeshStartIndex)
    {
        PushBackCurrentSubmesh(data);
        data.currentSubmeshStartIndex = data.indexData.size();
    }

    // Find material index in data.parsedMaterials matching mtlName
    size_t materialIndex = 0;
    for (size_t i = 0; i < data.parsedMaterials.size(); ++i)
    {
        if (data.parsedMaterials[i].name == mtlName)

        {
            materialIndex = i;
            break;
        }
    }
    data.currentSubMeshMaterial = materialIndex;
}

void PushBackCurrentSubmesh(ParseData& data)
{
    if (data.indexData.size() <= data.currentSubmeshStartIndex)
        return; // No indices for this submesh

    SubMeshInfo toAdd;
    toAdd.startIndexValue = data.currentSubmeshStartIndex;
    toAdd.nrOfIndicesInSubMesh = data.indexData.size() - toAdd.startIndexValue;
    toAdd.currentSubMeshMaterial = data.currentSubMeshMaterial;

    // Set texture SRVs to nullptr for now (no texture loading implemented)
    toAdd.ambientTextureSRV = nullptr;
    toAdd.diffuseTextureSRV = nullptr;
    toAdd.specularTextureSRV = nullptr;

    data.finishedSubMeshes.push_back(toAdd);
}

