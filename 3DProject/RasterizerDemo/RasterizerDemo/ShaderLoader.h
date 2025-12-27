#pragma once

#include <d3d11.h>
#include <string>
#include <vector>

class ShaderLoader
{
public:
    // Load compiled shader bytecode from a .cso file
    static std::vector<char> LoadCompiledShader(const std::string& filename);

    // Create shaders from .cso files
    static ID3D11VertexShader* CreateVertexShader(ID3D11Device* device, const std::string& filename, std::string* outByteCode = nullptr);
    static ID3D11PixelShader* CreatePixelShader(ID3D11Device* device, const std::string& filename);
    static ID3D11HullShader* CreateHullShader(ID3D11Device* device, const std::string& filename);
    static ID3D11DomainShader* CreateDomainShader(ID3D11Device* device, const std::string& filename);
    static ID3D11ComputeShader* CreateComputeShader(ID3D11Device* device, const std::string& filename);
};
