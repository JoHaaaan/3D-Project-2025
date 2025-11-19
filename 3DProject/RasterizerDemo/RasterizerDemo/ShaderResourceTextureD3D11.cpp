#include "ShaderResourceTextureD3D11.h"

ShaderResourceTextureD3D11::ShaderResourceTextureD3D11(ID3D11Device* device, UINT width, UINT height, void* textureData)
{
}

ShaderResourceTextureD3D11::ShaderResourceTextureD3D11(ID3D11Device* device, const char* pathToTextureFile)
{
}

ShaderResourceTextureD3D11::~ShaderResourceTextureD3D11()
{
}

void ShaderResourceTextureD3D11::Initialize(ID3D11Device* device, UINT width, UINT height, void* textureData)
{
}

void ShaderResourceTextureD3D11::Initialize(ID3D11Device* device, const char* pathToTextureFile)
{
}

ID3D11ShaderResourceView* ShaderResourceTextureD3D11::GetSRV() const
{
	return nullptr;
}
