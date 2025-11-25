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

	Mesh toAdd;
	// Use the data we have parsed into our ParseData strucutre and use it to build a mesh we can work with

	loadedMeshes[identifier] = toAdd;
}

void ParseLine(const std::string& line, ParseData& data)
{
	// Split the line into an identifier and data section

	// Use the identifier to determine what type of entry we are working with and parse the data section accordingly
	//...
	//...
	// Does identifier tell us it is a vertex position?
	// If so: ParsePosition(dataSectionString, data)
	//...
	//...

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

