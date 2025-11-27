#include "OBJParser.h"
#include "MeshD3D11.h"

float GetLineFloat(const std::string& line, size_t& currentLinePos)
{
    size_t numberStart = currentLinePos;

	while (currentLinePos < line.size() && line[currentLinePos] != ' ')
	{
		currentLinePos++;
	}

	std::string part = line.substr(numberStart, currentLinePos - numberStart);
	float extractedAndConvertedFloat = std::stof(part);

    // Loop until end of string or a space is reached, incrementing currentLinePos for each loop iteration

	// Exctract substring using numberStart and the incremented currentLinePos (tip: there is a fdunction in std::string for this, substr)

    // Convert the extracted substring to float (tip: there is a function in the standard libraru for this, std::stof)
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

    // Loop until end of string or either a '/' or space is reached, incrementing currentLinePos for each loop iteration

	// Extract substring using numberStart and the incremented currentLinePos (tip: there is a fdunction in std::string for this, substr)

	// Convert the extracted substring to int (tip: there is a function in the standard libraru for this, std::stoi)

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


	// Loop until end of string or a space is reached, incrementing currentLinePos for each loop iteration

	// Extract substring using stringStart and the incremented currentLinePos (tip: there is a function in std::string for this, substr)

	return extractedString;
}

const Mesh* GetMesh(const std::string& path)
{
	// loadedMeshes is our persistent map of meshes we have parsed

	if (loadedMeshes.find(path) == loadedMeshes.end())
	{
		std::string fileData;
		ReadFile(path, fileData);
		ParseOBJ(path, fileData);
	}

	return &loadedMeshes[path];
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

void ParseOBJ(const std::string& identifier, const std::string& contents)
{
	std::istringstream lineStream(contents);
	ParseData data;

	std::string line;
	while (std::getline(lineStream, line))
	{
		ParseLine(line, data);
	}

	PushBackCurrentSubmesh(data);

	MeshD3D11 toAdd;
	// Use the data we have parsed into our ParseData strucutre and use it to build a mesh we can work with

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
    float v = GetLineFloat(dataSection, pos);
    data.texCoords.push_back({ u, 1.0f - v }); // Invert V for DirectX
}

void ParseNormal(const std::string& dataSection, ParseData& data)
{
    size_t pos = 0;
    float x = GetLineFloat(dataSection, pos);
    float y = GetLineFloat(dataSection, pos);
    float z = GetLineFloat(dataSection, pos);
    data.normals.push_back({ x, y, -z }); // Invert Z
}

void ParseFace(const std::string& dataSection, ParseData& data)
{
    size_t pos = 0;

    // OBJ faces can be triangles or quads. We assume triangles or triangulated data.
    // If it's a quad, this loop will need adjustment (read 4, emit 2 triangles).
    // For now, we read 3 vertices.

    for (int i = 0; i < 3; ++i)
    {
        // Read the triplet string to use as a cache key (e.g., "1/1/1")
        size_t startToken = pos;
        while (pos < dataSection.size() && dataSection[pos] == ' ') pos++; // skip spaces
        size_t endToken = pos;
        while (endToken < dataSection.size() && dataSection[endToken] != ' ') endToken++;

        std::string token = dataSection.substr(pos, endToken - pos);

        // Check Cache
        if (data.vertexCache.find(token) != data.vertexCache.end())
        {
            data.indexData.push_back(data.vertexCache[token]);
            pos = endToken;
            continue;
        }

        // Not in cache, parse indices
        size_t subPos = pos;
        int vInd = GetLineInt(dataSection, subPos);
        int tInd = 0;
        int nInd = 0;

        // check for slash
        if (subPos < dataSection.size() && dataSection[subPos] == '/')
        {
            subPos++; // skip first slash
            if (subPos < dataSection.size() && dataSection[subPos] != '/')
            {
                tInd = GetLineInt(dataSection, subPos);
            }

            if (subPos < dataSection.size() && dataSection[subPos] == '/')
            {
                subPos++; // skip second slash
                nInd = GetLineInt(dataSection, subPos);
            }
        }
        pos = subPos; // Advance main position

        // Create Vertex
        Vertex newVert = {};

        // Handle negative indices (relative) and 1-based indices
        if (vInd < 0) vInd = (int)data.positions.size() + vInd + 1;
        if (tInd < 0) tInd = (int)data.texCoords.size() + tInd + 1;
        if (nInd < 0) nInd = (int)data.normals.size() + nInd + 1;

        if (vInd > 0) newVert.Position = data.positions[vInd - 1];
        if (tInd > 0) newVert.UV = data.texCoords[tInd - 1];
        if (nInd > 0) newVert.Normal = data.normals[nInd - 1];

        // Add to buffer and cache
        unsigned int newIndex = (unsigned int)data.vertices.size();
        data.vertices.push_back(newVert);
        data.indexData.push_back(newIndex);
        data.vertexCache[token] = newIndex;
    }
}

void ParseMtlLib(const std::string& dataSection, ParseData& data)
{
    // Simplified: Just store the file name to load later if needed, 
    // or parse it immediately. For this assignment, hardcoding or basic parsing is fine.
    // Implementing actual MTL parsing is a large task. 
    // Check if you have an external MTL loader or implement a basic one here.
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
    // (Implementation omitted for brevity, assumes materials are loaded)
}

void PushBackCurrentSubmesh(ParseData& data)
{
	SubMeshInfo toAdd;
	toAdd.startIndexValue = data.currentSubmeshStartIndex;
	toAdd.nrOfIndicesInSubMesh = data.indexData.size() - toAdd.startIndexValue;

	toAdd.ambientTextureSRV = loadedTextures[data.parsedMaterials[data.currentSubMeshMaterial].mapKa].GetSRV();
	toAdd.diffuseTextureSRV = loadedTextures[data.parsedMaterials[data.currentSubMeshMaterial].mapKd].GetSRV();
	toAdd.specularTextureSRV = loadedTextures[data.parsedMaterials[data.currentSubMeshMaterial].mapKs].GetSRV();

	data.finishedSubMeshes.push_back(toAdd);
}

