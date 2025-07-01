#include <Windows.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include "WindowHelper.h"
#include "D3D11Helper.h" 
#include "PipelineHelper.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include "RenderHelper.h"
#include "ConstantBufferD3D11.h"

using namespace DirectX;
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Transformation parameters
static const float FOV = XMConvertToRadians(45.0f);
static const float ASPECT_RATIO = 1280.0f / 720.0f;
static const float NEAR_PLANE = 0.1f;
static const float FAR_PLANE = 100.0f;

static const XMVECTOR EYE = XMVectorSet(0.0f, 0.0f, 3.0f, 1.0f);
static const XMVECTOR AT = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
static const XMVECTOR UP = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

static const XMMATRIX VIEW = XMMatrixLookAtLH(EYE, AT, UP);
static const XMMATRIX PROJECTION = XMMatrixPerspectiveFovLH(FOV, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE);
XMMATRIX VIEW_PROJ = VIEW * PROJECTION;

// Structures
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

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    const UINT WIDTH = 1024;
    const UINT HEIGHT = 576;
    HWND window;
    SetupWindow(hInstance, WIDTH, HEIGHT, nCmdShow, window);

    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

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
    ConstantBufferD3D11 constantBuffer;
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* textureView = nullptr;
    ID3D11SamplerState* samplerState = nullptr;
    ID3D11Buffer* lightBuffer = nullptr;
    ID3D11Buffer* materialBuffer = nullptr;
    ID3D11Buffer* cameraBuffer = nullptr;

    SetupD3D11(WIDTH, HEIGHT, window, device, immediateContext, swapChain, rtv, dsTexture, dsView, viewport);
    SetupPipeline(device, vertexBuffer, vShader, pShader, inputLayout);

    // Constant BUffer
    constantBuffer.Initialize(device, sizeof(MatrixPair));


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
        XMStoreFloat3(&cam.position, EYE);
        cam.padding = 0.f;
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(Camera);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SUBRESOURCE_DATA initData = { &cam };
        device->CreateBuffer(&desc, &initData, &cameraBuffer);
    }

    // Texture loading
    {
        int wTex, hTex, channels;
        unsigned char* imageData = stbi_load("image.png", &wTex, &hTex, &channels, 4);

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = wTex;
        texDesc.Height = hTex;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA texData = {};
        texData.pSysMem = imageData;
        texData.SysMemPitch = wTex * 4;

        device->CreateTexture2D(&texDesc, &texData, &texture);
        stbi_image_free(imageData);
        device->CreateShaderResourceView(texture, nullptr, &textureView);
    }

    // Sampler
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

    immediateContext->PSSetConstantBuffers(1, 1, &lightBuffer);
    immediateContext->PSSetConstantBuffers(2, 1, &materialBuffer);
    immediateContext->PSSetConstantBuffers(3, 1, &cameraBuffer);

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
        XMMATRIX worldMatrix = XMMatrixTranspose(lookAtMatrix) * translation;

        // Update matrix buffer
        MatrixPair data;
        XMStoreFloat4x4(&data.world, XMMatrixTranspose(worldMatrix));
        XMStoreFloat4x4(&data.viewProj, XMMatrixTranspose(VIEW_PROJ));
        constantBuffer.UpdateBuffer(immediateContext, &data);



        ID3D11Buffer* cb = constantBuffer.GetBuffer();
        Render(immediateContext, rtv, dsView, viewport, vShader, pShader, inputLayout, vertexBuffer, cb, textureView, samplerState, worldMatrix);


        swapChain->Present(0, 0);
    }

    // Manual cleanup (RAII handles constantBuffer)
    cameraBuffer->Release();
    materialBuffer->Release();
    lightBuffer->Release();
    samplerState->Release();
    textureView->Release();
    texture->Release();
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
