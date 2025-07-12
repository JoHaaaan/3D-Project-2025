#include "PipelineHelper.h"

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

bool CreateInputLayout(ID3D11Device* device, ID3D11InputLayout*& inputLayout, const std::string& vShaderByteCode)
{
    D3D11_INPUT_ELEMENT_DESC inputDesc[3] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    device->CreateInputLayout(inputDesc, 3, vShaderByteCode.c_str(), vShaderByteCode.size(), &inputLayout);
    return true;
}

bool CreateVertexBuffer(ID3D11Device* device, ID3D11Buffer*& vertexBuffer)
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

    

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = sizeof(quad);
    bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA data = { quad };
    device->CreateBuffer(&bufferDesc, &data, &vertexBuffer);
    return true;
}

bool SetupPipeline(ID3D11Device* device, ID3D11Buffer*& vertexBuffer, ID3D11VertexShader*& vShader,
    ID3D11PixelShader*& pShader, ID3D11InputLayout*& inputLayout)
{
    std::string vShaderByteCode;
    LoadShaders(device, vShader, pShader, vShaderByteCode);
    CreateInputLayout(device, inputLayout, vShaderByteCode);
    CreateVertexBuffer(device, vertexBuffer);
    return true;
}