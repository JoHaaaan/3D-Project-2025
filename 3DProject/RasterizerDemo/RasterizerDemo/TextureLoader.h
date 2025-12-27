#pragma once

#include <d3d11.h>
#include <string>

class TextureLoader
{
public:
    // Load a texture from file and create an SRV
    static ID3D11ShaderResourceView* LoadTexture(ID3D11Device* device, const std::string& filename);
    
    // Create a 1x1 solid color texture
    static ID3D11ShaderResourceView* CreateSolidColorTexture(ID3D11Device* device, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
    
    // Create a white fallback texture
    static ID3D11ShaderResourceView* CreateWhiteTexture(ID3D11Device* device);
};
