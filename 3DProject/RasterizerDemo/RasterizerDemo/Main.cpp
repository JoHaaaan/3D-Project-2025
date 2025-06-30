// Project

#include <Windows.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include "WindowHelper.h"
#include "D3D11Helper.h" 
#include "PipelineHelper.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include "CameraD3D11.h"
//#include "ConstantBufferD3D11.h"

using namespace DirectX;
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
// Transformation parameters
static const float FOV = XMConvertToRadians(45.0f);
static const float ASPECT_RATIO = 1280.0f / 720.0f;
static const float NEAR_PLANE = 0.1f;
static const float FAR_PLANE = 100.0f;



// Structures
struct MatrixPair {
    XMFLOAT4X4 world;
    XMFLOAT4X4 viewProj;
};

struct Light {
    XMFLOAT3 position;
    float    intensity;
    XMFLOAT3 color;
    float    padding; 
};

struct Material {
    XMFLOAT3 ambient;
    float    padding1;
    XMFLOAT3 diffuse;
    float    padding2;
    XMFLOAT3 specular;
    float    specularPower;
};

struct Camera {
    XMFLOAT3 position;
    float    padding;
};

// Render function
void Render(
    ID3D11DeviceContext* immediateContext,
    ID3D11RenderTargetView* rtv,
    ID3D11DepthStencilView* dsView,
    D3D11_VIEWPORT& viewport,
    ID3D11VertexShader* vShader,
    ID3D11PixelShader* pShader,
    ID3D11InputLayout* inputLayout,
    ID3D11Buffer* vertexBuffer,
    ID3D11Buffer* constantBuffer,
    ID3D11ShaderResourceView* textureView,
    ID3D11SamplerState* samplerState,
    const CameraD3D11& camera,
    const XMMATRIX& worldMatrix)
{
    float clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
    immediateContext->ClearRenderTargetView(rtv, clearColor);
    immediateContext->ClearDepthStencilView(dsView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

    // Transpose matrices
    XMMATRIX worldT = XMMatrixTranspose(worldMatrix);
    XMFLOAT4X4 viewProjMatrix = camera.GetViewProjectionMatrix();
    XMMATRIX viewProjT = XMMatrixTranspose(XMLoadFloat4x4(&camera.GetViewProjectionMatrix()));




    // Update constant buffer
    D3D11_MAPPED_SUBRESOURCE mapped;
    immediateContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

    MatrixPair data;
    XMStoreFloat4x4(&data.world, worldT);
    XMStoreFloat4x4(&data.viewProj, viewProjT);
    memcpy(mapped.pData, &data, sizeof(MatrixPair));

    immediateContext->Unmap(constantBuffer, 0);
    immediateContext->VSSetConstantBuffers(0, 1, &constantBuffer);

    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    immediateContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    immediateContext->IASetInputLayout(inputLayout);
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    immediateContext->VSSetShader(vShader, nullptr, 0);
    immediateContext->PSSetShader(pShader, nullptr, 0);
    immediateContext->RSSetViewports(1, &viewport);
    immediateContext->OMSetRenderTargets(1, &rtv, dsView);

    immediateContext->PSSetShaderResources(0, 1, &textureView);
    immediateContext->PSSetSamplers(0, 1, &samplerState);

    immediateContext->Draw(6, 0);
}


// main
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    // Window setup
    const UINT WIDTH = 1024;
    const UINT HEIGHT = 576;
    HWND window;
    SetupWindow(hInstance, WIDTH, HEIGHT, nCmdShow, window);

	// Debug memory leaks
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    // Device/SwapChain/Context
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* immediateContext = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11Texture2D* dsTexture = nullptr;
    ID3D11DepthStencilView* dsView = nullptr;
    D3D11_VIEWPORT viewport;
    ID3D11VertexShader* vShader = nullptr;
    ID3D11PixelShader* pShader = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* constantBuffer = nullptr;
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* textureView = nullptr;
    ID3D11SamplerState* samplerState = nullptr;
    ID3D11Buffer* lightBuffer = nullptr;
    ID3D11Buffer* materialBuffer = nullptr;
    ID3D11Buffer* cameraBuffer = nullptr;

    // Basic setup
    SetupD3D11(WIDTH, HEIGHT, window, device, immediateContext, swapChain, rtv, dsTexture, dsView, viewport);
    SetupPipeline(device, vertexBuffer, vShader, pShader, inputLayout);
    ProjectionInfo projInfo = {
    FOV,
    ASPECT_RATIO,
    NEAR_PLANE,
    FAR_PLANE
    };

    CameraD3D11 camera(device, projInfo, XMFLOAT3(0.0f, 0.0f, 3.0f));


    // MatrixPair buffer
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(MatrixPair);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&desc, nullptr, &constantBuffer);
    }

    // Light buffer
    {
        Light lightData{ XMFLOAT3(0.f, 0.f, 2.f), 1.f, XMFLOAT3(1.f, 1.f, 1.f), 0.f };
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(Light);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SUBRESOURCE_DATA initData = { &lightData };
        device->CreateBuffer(&desc, &initData, &lightBuffer);
    }

    // Material buffer
    {
        Material mat{
            XMFLOAT3(0.2f, 0.2f, 0.2f), 0.f,
            XMFLOAT3(0.5f, 0.5f, 0.5f), 0.f,
            XMFLOAT3(0.5f, 0.5f, 0.5f), 100.f
        };
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(Material);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SUBRESOURCE_DATA initData = { &mat };
        device->CreateBuffer(&desc, &initData, &materialBuffer);
    }

    // Camera buffer
    {
        Camera cam;
        cam.position = XMFLOAT3(0.0f, 0.0f, 3.0f);

        cam.padding = 0.f;
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(Camera);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SUBRESOURCE_DATA initData = { &cam };
        device->CreateBuffer(&desc, &initData, &cameraBuffer);
    }

    // Load Texture from File using stb_image
    {
        int wTex, hTex, channels;
        // Load "image.png" from disk as an RGBA image (4 channels)
        unsigned char* imageData = stbi_load("image.png", &wTex, &hTex, &channels, 4);

        // Describe the 2D texture to create
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = wTex;
        texDesc.Height = hTex;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        // Setup subresource data with the image loaded from disk
        D3D11_SUBRESOURCE_DATA texData = {};
        texData.pSysMem = imageData;
        texData.SysMemPitch = wTex * 4;
        // Create the texture on the GPU and copy the image data into it
        device->CreateTexture2D(&texDesc, &texData, &texture);

        // Free the image data from system memory after copying to GPU
        stbi_image_free(imageData);

        // Create a shader resource view so the texture can be accessed in the pixel shader
        device->CreateShaderResourceView(texture, nullptr, &textureView);
    }

    // Sampler state
    {
        D3D11_SAMPLER_DESC sdesc = {};
        sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sdesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sdesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sdesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sdesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sdesc.MinLOD = 0;
        sdesc.MaxLOD = D3D11_FLOAT32_MAX;
        device->CreateSamplerState(&sdesc, &samplerState);
    }

    // Bind extra constant buffers to the pixel shader
    immediateContext->PSSetConstantBuffers(1, 1, &lightBuffer);
    immediateContext->PSSetConstantBuffers(2, 1, &materialBuffer);
    immediateContext->PSSetConstantBuffers(3, 1, &cameraBuffer);

	// Main loop
    auto previousTime = std::chrono::high_resolution_clock::now();
    float rotationAngle = 0.f;
    MSG msg = {};
    while (!(GetKeyState(VK_ESCAPE) & 0x8000) && msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(currentTime - previousTime).count();
        previousTime = currentTime;

        rotationAngle += XMConvertToRadians(-60.f) * dt;
        if (rotationAngle > XM_2PI) rotationAngle -= XM_2PI;

        float armLength = 0.7f;
        float x = armLength * cosf(rotationAngle);
        float z = armLength * sinf(rotationAngle);

        XMMATRIX translation = XMMatrixTranslation(x, 0.f, z);
        XMVECTOR objectPos = XMVectorSet(x, 0.f, z, 1.f);
        XMMATRIX lookAtMatrix = XMMatrixLookAtLH(objectPos, XMVectorZero(), XMVectorSet(0.f, 1.f, 0.f, 0.f));

        // Combine rotation/orientation (transposing to use it as a 'world' transform)
        XMMATRIX worldMatrix = XMMatrixTranspose(lookAtMatrix) * translation;

        Render(immediateContext, rtv, dsView, viewport, vShader, pShader, inputLayout,
            vertexBuffer, constantBuffer, textureView, samplerState, camera, worldMatrix);


        swapChain->Present(0, 0);
    }

    // Cleanup
    cameraBuffer->Release();
    materialBuffer->Release();
    lightBuffer->Release();
    samplerState->Release();
    textureView->Release();
    texture->Release();
    constantBuffer->Release();
    vertexBuffer->Release();
    inputLayout->Release();
    pShader->Release();
    vShader->Release();
    dsView->Release();
    dsTexture->Release();
    rtv->Release();
    swapChain->Release();
    immediateContext->Release();
    device->Release();

    return 0;
}