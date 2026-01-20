#include "TextureLoader.h"
#include <Windows.h>

extern "C" {
    unsigned char* stbi_load(char const* filename, int* x, int* y, int* channels_in_file, int desired_channels);
    void stbi_image_free(void* retval_from_stbi_load);
}

ID3D11ShaderResourceView* TextureLoader::LoadTexture(ID3D11Device* device, const std::string& filename)
{
    int width, height, channels;
    unsigned char* imageData = stbi_load(filename.c_str(), &width, &height, &channels, 4);

    if (!imageData)
    {
        std::string msg = "Failed to load texture: " + filename + "\n";
        OutputDebugStringA(msg.c_str());
        return nullptr;
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA texData = {};
    texData.pSysMem = imageData;
    texData.SysMemPitch = width * 4;

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = device->CreateTexture2D(&texDesc, &texData, &texture);
    stbi_image_free(imageData);

    if (FAILED(hr))
    {
        std::string msg = "Failed to create texture: " + filename + "\n";
        OutputDebugStringA(msg.c_str());
        return nullptr;
    }

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device->CreateShaderResourceView(texture, nullptr, &srv);
    texture->Release();

    if (FAILED(hr))
    {
        std::string msg = "Failed to create SRV for texture: " + filename + "\n";
        OutputDebugStringA(msg.c_str());
        return nullptr;
    }

    std::string msg = "Texture loaded: " + filename + "\n";
    OutputDebugStringA(msg.c_str());
    return srv;
}

ID3D11ShaderResourceView* TextureLoader::CreateSolidColorTexture(ID3D11Device* device, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    unsigned char pixel[4] = { r, g, b, a };

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA texData = {};
    texData.pSysMem = pixel;
    texData.SysMemPitch = 4;

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = device->CreateTexture2D(&texDesc, &texData, &texture);

    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create solid color texture\n");
        return nullptr;
    }

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device->CreateShaderResourceView(texture, nullptr, &srv);
    texture->Release();

    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create SRV for solid color texture\n");
        return nullptr;
    }

    return srv;
}

ID3D11ShaderResourceView* TextureLoader::CreateWhiteTexture(ID3D11Device* device)
{
    return CreateSolidColorTexture(device, 255, 255, 255, 255);
}
