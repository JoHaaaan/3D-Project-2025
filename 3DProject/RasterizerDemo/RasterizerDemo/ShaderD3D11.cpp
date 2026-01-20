#include "ShaderD3D11.h"

ShaderD3D11::~ShaderD3D11()
{
	// Release the shader blob first
	if (shaderBlob)
	{
		shaderBlob->Release();
		shaderBlob = nullptr;
	}

	// Release the appropriate shader based on type
	// Since shader is a union, we only need to check if the first member is not null
	if (shader.vertex)
	{
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
		shader.vertex = nullptr;
	}
}

ShaderD3D11::ShaderD3D11(ID3D11Device* device, ShaderType shaderType, const void* dataPtr, size_t dataSize)
{
}

ShaderD3D11::ShaderD3D11(ID3D11Device* device, ShaderType shaderType, const char* csoPath)
{
}

void ShaderD3D11::Initialize(ID3D11Device* device, ShaderType shaderType, const void* dataPtr, size_t dataSize)
{
}

void ShaderD3D11::Initialize(ID3D11Device* device, ShaderType shaderType, const char* csoPath)
{
}

const void* ShaderD3D11::GetShaderByteData() const
{
	return nullptr;
}

size_t ShaderD3D11::GetShaderByteSize() const
{
	return size_t();
}

void ShaderD3D11::BindShader(ID3D11DeviceContext* context) const
{
}
