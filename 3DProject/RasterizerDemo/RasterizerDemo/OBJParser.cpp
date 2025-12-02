#include "OBJParser.h"
#include "MeshD3D11.h"
#include "stb_image.h" // header only (STB_IMAGE_IMPLEMENTATION present in one TU - Main.cpp)

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

    // Helper to load a texture file into an SRV (returns nullptr on failure)
    auto LoadTextureSRV = [&](const std::string& texPath) -> ID3D11ShaderResourceView*
    {
        if (texPath.empty())
            return nullptr;

        std::string fullPath = defaultDirectory + texPath;
        
        // Debug output
        std::string debugMsg = "Attempting to load texture: " + fullPath + "\n";
        OutputDebugStringA(debugMsg.c_str());
        
        int w = 0, h = 0, channels = 0;
        unsigned char* imageData = stbi_load(fullPath.c_str(), &w, &h, &channels, 4);
      if (!imageData)
        {
            // failed to load image
 std::string errorMsg = "FAILED to load texture: " + fullPath + "\n";
      OutputDebugStringA(errorMsg.c_str());
            return nullptr;
      }

        std::string successMsg = "SUCCESS: Loaded texture " + fullPath + " (" + std::to_string(w) + "x" + std::to_string(h) + ")\n";
    OutputDebugStringA(successMsg.c_str());

        D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = static_cast<UINT>(w);
        desc.Height = static_cast<UINT>(h);
    desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA texData = {};
        texData.pSysMem = imageData;
        texData.SysMemPitch = w * 4;

        ID3D11Texture2D* tex = nullptr;
        HRESULT hr = device->CreateTexture2D(&desc, &texData, &tex);
 stbi_image_free(imageData);

        if (FAILED(hr) || !tex)
 {
            if (tex) tex->Release();
          OutputDebugStringA("FAILED to create D3D11 texture\n");
            return nullptr;
        }

        ID3D11ShaderResourceView* srv = nullptr;
        hr = device->CreateShaderResourceView(tex, nullptr, &srv);
        // We can release the local texture; the SRV holds a reference.
        tex->Release();

  if (FAILED(hr))
      {
   if (srv) srv->Release();
            OutputDebugStringA("FAILED to create SRV for texture\n");
    return nullptr;
        }

   OutputDebugStringA("SUCCESS: Created SRV for texture\n");
        return srv;
    };

    // 4. Fill SubMesh Info
    for (const auto& sub : data.finishedSubMeshes)
    {
        MeshData::SubMeshInfo sm = {};
        sm.startIndexValue = sub.startIndexValue;
        sm.nrOfIndicesInSubMesh = sub.nrOfIndicesInSubMesh;
        sm.ambientTextureSRV = nullptr;  // no ambient map handling yet
        sm.diffuseTextureSRV = nullptr;
        sm.specularTextureSRV = nullptr;
        sm.materialIndex = sub.currentSubMeshMaterial;

        const auto& material = data.parsedMaterials[sm.materialIndex];
        sm.material.ambient = material.ambient;
        sm.material.diffuse = material.diffuse;
        sm.material.specular = material.specular;
     sm.material.specularPower = material.specularPower;

        // Debug: Print material info
{
            char buf[512];
    sprintf_s(buf, "Submesh %zu: Material '%s', map_Kd='%s'\n",
  meshInfo.subMeshInfo.size(),
             material.name.c_str(),
     material.mapKd.c_str());
         OutputDebugStringA(buf);
   }

   // If material references a diffuse texture, try loading it.
if (!material.mapKd.empty())
  {
  ID3D11ShaderResourceView* diffuseSRV = LoadTextureSRV(material.mapKd);
    sm.diffuseTextureSRV = diffuseSRV; // may be nullptr on failure
        }
        else
        {
  OutputDebugStringA("  -> No map_Kd specified for this material\n");
        }

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
                if (tInd < 0) tInd = (int)data.texCoords.size() + vInd + 1;
                if (nInd < 0) nInd = (int)data.normals.size() + vInd + 1;

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
    
    // Debug: print what material is being requested
    {
      std::string debugMsg = "usemtl: Requesting material '" + mtlName + "'\n";
        OutputDebugStringA(debugMsg.c_str());
    }

    // If we have indices, push the previous submesh
    if (data.indexData.size() > data.currentSubmeshStartIndex)
    {
        PushBackCurrentSubmesh(data);
   data.currentSubmeshStartIndex = data.indexData.size();
    }

    // Find material index in data.parsedMaterials matching mtlName
    size_t materialIndex = 0;
    bool found = false;
    for (size_t i = 0; i < data.parsedMaterials.size(); ++i)
    {
        if (data.parsedMaterials[i].name == mtlName)
        {
  materialIndex = i;
            found = true;
  break;
    }
    }
    
    if (!found)
    {
        std::string debugMsg = "  WARNING: Material '" + mtlName + "' not found in parsed materials!\n";
        OutputDebugStringA(debugMsg.c_str());
      
      // Print all available materials
   OutputDebugStringA("  Available materials:\n");
        for (size_t i = 0; i < data.parsedMaterials.size(); ++i)
        {
    std::string matDebug = "    [" + std::to_string(i) + "] '" + data.parsedMaterials[i].name + "'\n";
    OutputDebugStringA(matDebug.c_str());
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

