#include "PipelineHelper.h"
#include "VertexBufferD3D11.h"

#include <fstream>
#include <string>
#include <vector>

bool LoadShaders(ID3D11Device* device, ID3D11VertexShader*& vShader, ID3D11PixelShader*& pShader, std::string& vShaderByteCode)
{
    auto loadShader = [](const std::string& filename) -> std::vector<char>
    {
        std::ifstream reader(filename, std::ios::binary | std::ios::ate);
        size_t size = reader.tellg();
        std::vector<char> buffer(size);
        reader.seekg(0);
        reader.read(buffer.data(), size);
        return buffer;
    };

    auto vertexShaderData = loadShader("VertexShader.cso");
    device->CreateVertexShader(vertexShaderData.data(), vertexShaderData.size(), nullptr, &vShader);
    vShaderByteCode.assign(vertexShaderData.begin(), vertexShaderData.end());

    auto pixelShaderData = loadShader("PixelShader.cso");
    device->CreatePixelShader(pixelShaderData.data(), pixelShaderData.size(), nullptr, &pShader);

    return true;
}

bool CreateVertexBuffer(ID3D11Device* device, VertexBufferD3D11& vertexBuffer)
{
    SimpleVertex quad[] =
    {
        // First triangle
        { {-0.5f,  0.5f, 0.0f}, {0, 0, -1}, {0.0f, 0.0f} },
        { { 0.5f, -0.5f, 0.0f}, {0, 0, -1}, {1.0f, 1.0f} },
        { {-0.5f, -0.5f, 0.0f}, {0, 0, -1}, {0.0f, 1.0f} },

        // Second triangle
        { {-0.5f,  0.5f, 0.0f}, {0, 0, -1}, {0.0f, 0.0f} },
        { { 0.5f,  0.5f, 0.0f}, {0, 0, -1}, {1.0f, 0.0f} },
        { { 0.5f, -0.5f, 0.0f}, {0, 0, -1}, {1.0f, 1.0f} }
    };

    

    const UINT vertexCount = static_cast<UINT>(sizeof(quad) / sizeof(quad[0]));
    vertexBuffer.Initialize(device, sizeof(SimpleVertex), vertexCount, quad);
    return true;
}

bool SetupPipeline(ID3D11Device* device, VertexBufferD3D11& vertexBuffer, ID3D11VertexShader*& vShader,
    ID3D11PixelShader*& pShader, std::string& outVertexShaderByteCode)
{
    LoadShaders(device, vShader, pShader, outVertexShaderByteCode);
    CreateVertexBuffer(device, vertexBuffer);
    return true;
}