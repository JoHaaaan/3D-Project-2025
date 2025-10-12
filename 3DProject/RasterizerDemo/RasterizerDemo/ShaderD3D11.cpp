#include "ShaderD3D11.h"
#include <d3dcompiler.h>
#include <fstream>
#include <vector>

ShaderD3D11::ShaderD3D11(ID3D11Device* device, ShaderType shaderType, const void* dataPtr, size_t dataSize)
	: type(shaderType)
{
	Initialize(device, shaderType, dataPtr, dataSize);
}

ShaderD3D11::ShaderD3D11(ID3D11Device* device, ShaderType shaderType, const char* csoPath)
	: type(shaderType)
{
	Initialize(device, shaderType, csoPath);
}

ShaderD3D11::~ShaderD3D11()
{
	if (shaderBlob)
		shaderBlob->Release();

	// Release the appropriate shader type
	switch (type)
	{
	case ShaderType::VERTEX_SHADER:
		if (shader.vertex) shader.vertex->Release();
		break;
	case ShaderType::HULL_SHADER:
		if (shader.hull) shader.hull->Release();
		break;
	case ShaderType::DOMAIN_SHADER:
		if (shader.domain) shader.domain->Release();
		break;
	case ShaderType::GEOMETRY_SHADER:
		if (shader.geometry) shader.geometry->Release();
		break;
	case ShaderType::PIXEL_SHADER:
		if (shader.pixel) shader.pixel->Release();
		break;
	case ShaderType::COMPUTE_SHADER:
		if (shader.compute) shader.compute->Release();
		break;
	}
}

void ShaderD3D11::Initialize(ID3D11Device* device, ShaderType shaderType, const void* dataPtr, size_t dataSize)
{
	type = shaderType;

	// Create blob from data
	D3DCreateBlob(dataSize, &shaderBlob);
	memcpy(shaderBlob->GetBufferPointer(), dataPtr, dataSize);

	// Create the appropriate shader type
	switch (type)
	{
	case ShaderType::VERTEX_SHADER:
		device->CreateVertexShader(dataPtr, dataSize, nullptr, &shader.vertex);
		break;
	case ShaderType::HULL_SHADER:
		device->CreateHullShader(dataPtr, dataSize, nullptr, &shader.hull);
		break;
	case ShaderType::DOMAIN_SHADER:
		device->CreateDomainShader(dataPtr, dataSize, nullptr, &shader.domain);
		break;
	case ShaderType::GEOMETRY_SHADER:
		device->CreateGeometryShader(dataPtr, dataSize, nullptr, &shader.geometry);
		break;
	case ShaderType::PIXEL_SHADER:
		device->CreatePixelShader(dataPtr, dataSize, nullptr, &shader.pixel);
		break;
	case ShaderType::COMPUTE_SHADER:
		device->CreateComputeShader(dataPtr, dataSize, nullptr, &shader.compute);
		break;
	}
}

void ShaderD3D11::Initialize(ID3D11Device* device, ShaderType shaderType, const char* csoPath)
{
	type = shaderType;

	// Load shader bytecode from file
	std::ifstream file(csoPath, std::ios::binary | std::ios::ate);
	size_t size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	file.read(buffer.data(), size);
	file.close();

	// Create blob from file data
	D3DCreateBlob(size, &shaderBlob);
	memcpy(shaderBlob->GetBufferPointer(), buffer.data(), size);

	// Create the appropriate shader type
	switch (type)
	{
	case ShaderType::VERTEX_SHADER:
		device->CreateVertexShader(buffer.data(), size, nullptr, &shader.vertex);
		break;
	case ShaderType::HULL_SHADER:
		device->CreateHullShader(buffer.data(), size, nullptr, &shader.hull);
		break;
	case ShaderType::DOMAIN_SHADER:
		device->CreateDomainShader(buffer.data(), size, nullptr, &shader.domain);
		break;
	case ShaderType::GEOMETRY_SHADER:
		device->CreateGeometryShader(buffer.data(), size, nullptr, &shader.geometry);
		break;
	case ShaderType::PIXEL_SHADER:
		device->CreatePixelShader(buffer.data(), size, nullptr, &shader.pixel);
		break;
	case ShaderType::COMPUTE_SHADER:
		device->CreateComputeShader(buffer.data(), size, nullptr, &shader.compute);
		break;
	}
}

const void* ShaderD3D11::GetShaderByteData() const
{
	return shaderBlob ? shaderBlob->GetBufferPointer() : nullptr;
}

size_t ShaderD3D11::GetShaderByteSize() const
{
	return shaderBlob ? shaderBlob->GetBufferSize() : 0;
}

void ShaderD3D11::BindShader(ID3D11DeviceContext* context) const
{
	switch (type)
	{
	case ShaderType::VERTEX_SHADER:
		context->VSSetShader(shader.vertex, nullptr, 0);
		break;
	case ShaderType::HULL_SHADER:
		context->HSSetShader(shader.hull, nullptr, 0);
		break;
	case ShaderType::DOMAIN_SHADER:
		context->DSSetShader(shader.domain, nullptr, 0);
		break;
	case ShaderType::GEOMETRY_SHADER:
		context->GSSetShader(shader.geometry, nullptr, 0);
		break;
	case ShaderType::PIXEL_SHADER:
		context->PSSetShader(shader.pixel, nullptr, 0);
		break;
	case ShaderType::COMPUTE_SHADER:
		context->CSSetShader(shader.compute, nullptr, 0);
		break;
	}
}
