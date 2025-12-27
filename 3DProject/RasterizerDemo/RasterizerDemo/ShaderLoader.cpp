#include "ShaderLoader.h"
#include <fstream>
#include <Windows.h>

std::vector<char> ShaderLoader::LoadCompiledShader(const std::string& filename)
{
    std::ifstream reader(filename, std::ios::binary | std::ios::ate);
    if (!reader)
    {
        std::string msg = "Shader file not found: " + filename + "\n";
        OutputDebugStringA(msg.c_str());
        return {};
    }

    size_t size = static_cast<size_t>(reader.tellg());
    std::vector<char> buffer(size);
    reader.seekg(0);
    reader.read(buffer.data(), size);
    return buffer;
}

ID3D11VertexShader* ShaderLoader::CreateVertexShader(ID3D11Device* device, const std::string& filename, std::string* outByteCode)
{
    auto data = LoadCompiledShader(filename);
    if (data.empty())
        return nullptr;

    ID3D11VertexShader* shader = nullptr;
    HRESULT hr = device->CreateVertexShader(data.data(), data.size(), nullptr, &shader);

    if (FAILED(hr))
    {
        std::string msg = "Failed to create vertex shader: " + filename + "\n";
        OutputDebugStringA(msg.c_str());
        return nullptr;
    }

    if (outByteCode)
    {
        outByteCode->assign(data.begin(), data.end());
    }

    std::string msg = "Vertex shader loaded: " + filename + "\n";
    OutputDebugStringA(msg.c_str());
    return shader;
}

ID3D11PixelShader* ShaderLoader::CreatePixelShader(ID3D11Device* device, const std::string& filename)
{
    auto data = LoadCompiledShader(filename);
    if (data.empty())
    return nullptr;

    ID3D11PixelShader* shader = nullptr;
    HRESULT hr = device->CreatePixelShader(data.data(), data.size(), nullptr, &shader);

    if (FAILED(hr))
    {
        std::string msg = "Failed to create pixel shader: " + filename + "\n";
        OutputDebugStringA(msg.c_str());
        return nullptr;
    }

    std::string msg = "Pixel shader loaded: " + filename + "\n";
    OutputDebugStringA(msg.c_str());
    return shader;
}

ID3D11HullShader* ShaderLoader::CreateHullShader(ID3D11Device* device, const std::string& filename)
{
    auto data = LoadCompiledShader(filename);
    if (data.empty())
        return nullptr;

    ID3D11HullShader* shader = nullptr;
    HRESULT hr = device->CreateHullShader(data.data(), data.size(), nullptr, &shader);

    if (FAILED(hr))
    {
        std::string msg = "Failed to create hull shader: " + filename + "\n";
        OutputDebugStringA(msg.c_str());
        return nullptr;
    }

    std::string msg = "Hull shader loaded: " + filename + "\n";
    OutputDebugStringA(msg.c_str());
    return shader;
}

ID3D11DomainShader* ShaderLoader::CreateDomainShader(ID3D11Device* device, const std::string& filename)
{
    auto data = LoadCompiledShader(filename);
    if (data.empty())
        return nullptr;

    ID3D11DomainShader* shader = nullptr;
    HRESULT hr = device->CreateDomainShader(data.data(), data.size(), nullptr, &shader);

    if (FAILED(hr))
    {
        std::string msg = "Failed to create domain shader: " + filename + "\n";
        OutputDebugStringA(msg.c_str());
        return nullptr;
    }

    std::string msg = "Domain shader loaded: " + filename + "\n";
    OutputDebugStringA(msg.c_str());
    return shader;
}

ID3D11ComputeShader* ShaderLoader::CreateComputeShader(ID3D11Device* device, const std::string& filename)
{
    auto data = LoadCompiledShader(filename);
    if (data.empty())
        return nullptr;

    ID3D11ComputeShader* shader = nullptr;
    HRESULT hr = device->CreateComputeShader(data.data(), data.size(), nullptr, &shader);

    if (FAILED(hr))
    {
        std::string msg = "Failed to create compute shader: " + filename + "\n";
        OutputDebugStringA(msg.c_str());
        return nullptr;
    }

    std::string msg = "Compute shader loaded: " + filename + "\n";
    OutputDebugStringA(msg.c_str());
    return shader;
}
