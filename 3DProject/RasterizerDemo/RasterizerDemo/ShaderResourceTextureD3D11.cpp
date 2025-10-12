#include "ShaderResourceTextureD3D11.h"
#include "stb_image.h"

ShaderResourceTextureD3D11::ShaderResourceTextureD3D11(ID3D11Device* device, UINT width, UINT height, void* textureData)
{
	Initialize(device, width, height, textureData);
}

ShaderResourceTextureD3D11::ShaderResourceTextureD3D11(ID3D11Device* device, const char* pathToTextureFile)
{
	Initialize(device, pathToTextureFile);
}

ShaderResourceTextureD3D11::~ShaderResourceTextureD3D11()
{
	if (srv)
		srv->Release();
	if (texture)
		texture->Release();
}

void ShaderResourceTextureD3D11::Initialize(ID3D11Device* device, UINT width, UINT height, void* textureData)
{
	// Texture description
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = textureData;
	initData.SysMemPitch = width * 4; // 4 bytes per pixel (RGBA)

	device->CreateTexture2D(&texDesc, &initData, &texture);

	// Create shader resource view
	device->CreateShaderResourceView(texture, nullptr, &srv);
}

void ShaderResourceTextureD3D11::Initialize(ID3D11Device* device, const char* pathToTextureFile)
{
	int width, height, channels;
	unsigned char* imageData = stbi_load(pathToTextureFile, &width, &height, &channels, 4);

	if (imageData)
	{
		Initialize(device, static_cast<UINT>(width), static_cast<UINT>(height), imageData);
		stbi_image_free(imageData);
	}
}

ID3D11ShaderResourceView* ShaderResourceTextureD3D11::GetSRV() const
{
	return srv;
}
