#include "ShaderResourceTextureD3D11.h"
#include "stb_image.h"
#include <iostream>
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
	{
		srv->Release();
		srv = nullptr;
	}

	if (texture)
	{
		texture->Release();
		texture = nullptr;
	}
}

void ShaderResourceTextureD3D11::Initialize(ID3D11Device* device, UINT width, UINT height, void* textureData)
{
	if (srv)
	{
		srv->Release();
		srv = nullptr;
	}

	if (texture)
	{
		texture->Release();
		texture = nullptr;
	}

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    // We assume 4 channels (RGBA) 8-bit per channel for standard textures
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = textureData;
    // Pitch is the width of one row in bytes. 4 bytes per pixel.
    initData.SysMemPitch = width * 4;

    HRESULT hr = device->CreateTexture2D(&desc, &initData, &texture);
    if (FAILED(hr))
    {
        // Simple error logging
        OutputDebugStringA("Failed to create Texture2D in ShaderResourceTextureD3D11\n");
        return;
    }

    hr = device->CreateShaderResourceView(texture, nullptr, &srv);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create SRV in ShaderResourceTextureD3D11\n");
    }

}

void ShaderResourceTextureD3D11::Initialize(ID3D11Device* device, const char* pathToTextureFile)
{
    int width, height, channels;
    // Force 4 channels so it matches DXGI_FORMAT_R8G8B8A8_UNORM
    unsigned char* data = stbi_load(pathToTextureFile, &width, &height, &channels, 4);

    if (data)
    {
        Initialize(device, static_cast<UINT>(width), static_cast<UINT>(height), data);
        stbi_image_free(data);
    }
    else
    {
        std::string errorMsg = "Failed to load image: ";
        errorMsg += pathToTextureFile;
        errorMsg += "\n";
        OutputDebugStringA(errorMsg.c_str());
    }
}

ID3D11ShaderResourceView* ShaderResourceTextureD3D11::GetSRV() const
{
    return srv;
}
