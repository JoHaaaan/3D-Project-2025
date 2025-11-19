#include "ShaderD3D11.h"

ShaderD3D11::~ShaderD3D11()
{
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
