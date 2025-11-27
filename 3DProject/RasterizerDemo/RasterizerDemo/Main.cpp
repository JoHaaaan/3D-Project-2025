#include <Windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include "WindowHelper.h"
#include "D3D11Helper.h" 
#include "PipelineHelper.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include "RenderHelper.h"
#include "ConstantBufferD3D11.h"
#include "CameraD3D11.h"
#include "SamplerD3D11.h"
#include "InputLayoutD3D11.h"
#include "VertexBufferD3D11.h"
#include "DepthBufferD3D11.h"
#include "RenderTargetD3D11.h"
#include "OBJParser.h"
#include "MeshD3D11.h"
#include "GBufferD3D11.h"          // <<< NEW

using namespace DirectX;
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Transformation parameters
static const float FOV = XMConvertToRadians(45.0f);
static const float ASPECT_RATIO = 1280.0f / 720.0f;
static const float NEAR_PLANE = 0.1f;
static const float FAR_PLANE = 100.0f;

XMMATRIX PROJECTION = XMMatrixPerspectiveFovLH(FOV, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE);
XMMATRIX VIEW_PROJ;

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

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    const UINT WIDTH = 1024;
    const UINT HEIGHT = 576;
    HWND window;
    SetupWindow(hInstance, WIDTH, HEIGHT, nCmdShow, window);

    // center point in screen coords
    POINT center = { WIDTH / 2, HEIGHT / 2 };
    ClientToScreen(window, &center);
    ShowCursor(FALSE);
    SetCursorPos(center.x, center.y);

    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* immediateContext = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;  // backbuffer RTV, används bara ev. senare
    D3D11_VIEWPORT viewport;
    ID3D11VertexShader* vShader = nullptr;
    ID3D11PixelShader* pShader = nullptr;
    InputLayoutD3D11 inputLayout;
    VertexBufferD3D11 vertexBuffer;
    ConstantBufferD3D11 constantBuffer;
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* textureView = nullptr;
    SamplerD3D11 samplerState;
    ConstantBufferD3D11 lightBuffer;
    ConstantBufferD3D11 materialBuffer;
    CameraD3D11 camera;

    SetupD3D11(WIDTH, HEIGHT, window, device, immediateContext, swapChain, rtv, viewport);

    std::string vShaderByteCode;
    SetupPipeline(device, vertexBuffer, vShader, pShader, vShaderByteCode);

    // Depth buffer wrapper
    DepthBufferD3D11 depthBuffer(device, WIDTH, HEIGHT, false);

    // Offscreen render target (som tidigare)
    RenderTargetD3D11 sceneRT;
    sceneRT.Initialize(device, WIDTH, HEIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, false);

    // <<< NEW: G-buffer-instans
    GBufferD3D11 gbuffer;
    gbuffer.Initialize(device, WIDTH, HEIGHT);

    inputLayout.AddInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT);
    inputLayout.AddInputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT);
    inputLayout.AddInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);
    inputLayout.FinalizeInputLayout(device, vShaderByteCode.data(), vShaderByteCode.size());

    // Triangle for testing
    VertexBufferD3D11 triangleVB;
    {
        SimpleVertex triVerts[] = {
            { {  0.0f,  3.0f, 0.0f }, {0,0,-1}, {0.0f, 0.0f} },
            { {  3.0f, -3.0f, 0.0f }, {0,0,-1}, {1.0f, 1.0f} },
            { { -3.0f, -3.0f, 0.0f }, {0,0,-1}, {0.0f, 1.0f} },
        };
        triangleVB.Initialize(
            device,
            sizeof(SimpleVertex),
            static_cast<UINT>(sizeof(triVerts) / sizeof(triVerts[0])),
            triVerts);
    }

    // Constant Buffer
    constantBuffer.Initialize(device, sizeof(MatrixPair));

    // Light buffer
    Light lightData{ XMFLOAT3(0.f, 0.f, -2.f), 1.f, XMFLOAT3(1.f, 1.f, 1.f), 0.f };
    lightBuffer.Initialize(device, sizeof(Light), &lightData);

    // Note: 'GetMesh' is a global function from OBJParser.h
    const MeshD3D11* cubeMesh = GetMesh("cube.obj", device);

    // Material buffer
    Material mat{
        XMFLOAT3(0.2f, 0.2f, 0.2f), 0.f,
        XMFLOAT3(0.5f, 0.5f, 0.5f), 0.f,
        XMFLOAT3(0.5f, 0.5f, 0.5f), 100.f
    };
    materialBuffer.Initialize(device, sizeof(Material), &mat);

    // Camera setup
    ProjectionInfo proj{ FOV, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE };
    camera.Initialize(device, proj, XMFLOAT3(0.0f, 0.0f, -10.0f));

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

    samplerState.Initialize(device, D3D11_TEXTURE_ADDRESS_WRAP);

    ID3D11Buffer* lightCB = lightBuffer.GetBuffer();
    ID3D11Buffer* materialCB = materialBuffer.GetBuffer();
    ID3D11Buffer* cameraCB = camera.GetConstantBuffer();

    immediateContext->PSSetConstantBuffers(1, 1, &lightCB);
    immediateContext->PSSetConstantBuffers(2, 1, &materialCB);
    immediateContext->PSSetConstantBuffers(3, 1, &cameraCB);

    auto previousTime = std::chrono::high_resolution_clock::now();
    float rotationAngle = 90.f;

    const float mouseSens = 0.1f;

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

        rotationAngle += XMConvertToRadians(30.f) * dt;
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

        //  Camera movement + mouselook
        const float camSpeed = 3.0f;
        if (GetAsyncKeyState('W') & 0x8000) camera.MoveForward(camSpeed * dt);
        if (GetAsyncKeyState('S') & 0x8000) camera.MoveForward(-camSpeed * dt);
        if (GetAsyncKeyState('A') & 0x8000) camera.MoveRight(-camSpeed * dt);
        if (GetAsyncKeyState('D') & 0x8000) camera.MoveRight(camSpeed * dt);
        if (GetAsyncKeyState('Q') & 0x8000) camera.MoveUp(-camSpeed * dt);
        if (GetAsyncKeyState('E') & 0x8000) camera.MoveUp(camSpeed * dt);

        POINT mouseP;
        GetCursorPos(&mouseP);
        int dx = mouseP.x - center.x;
        int dy = mouseP.y - center.y;

        SetCursorPos(center.x, center.y);

        camera.RotateRight(XMConvertToRadians(dx * mouseSens));
        camera.RotateForward(XMConvertToRadians(dy * mouseSens));
        camera.UpdateInternalConstantBuffer(immediateContext);

        XMFLOAT4X4 vp = camera.GetViewProjectionMatrix();
        VIEW_PROJ = XMLoadFloat4x4(&vp);

        // Debug print camera position
        {
            XMFLOAT3 p = camera.GetPosition();
            char buf[128];
            sprintf_s(buf, "Camera Pos: X=%.3f  Y=%.3f  Z=%.3f\n", p.x, p.y, p.z);
            OutputDebugStringA(buf);
        }

        ID3D11DepthStencilView* myDSV = depthBuffer.GetDSV(0);
        ID3D11RenderTargetView* sceneRTV = sceneRT.GetRTV();

        // ----- FORWARD-PASS TILL sceneRT (oförändrat) -----
        Render(immediateContext, sceneRTV, myDSV, viewport,
            vShader, pShader, inputLayout.GetInputLayout(), vertexBuffer.GetBuffer(),
            constantBuffer.GetBuffer(), textureView,
            samplerState.GetSamplerState(), worldMatrix);

        // --- CUBE RENDERING (som tidigare, ritar också till sceneRT) ---

        ID3D11Buffer* cb0 = constantBuffer.GetBuffer();
        immediateContext->VSSetConstantBuffers(0, 1, &cb0);

        immediateContext->IASetInputLayout(inputLayout.GetInputLayout());
        immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        immediateContext->VSSetShader(vShader, nullptr, 0);
        immediateContext->PSSetShader(pShader, nullptr, 0);

        immediateContext->PSSetShaderResources(0, 1, &textureView);
        ID3D11SamplerState* samplerPtr = samplerState.GetSamplerState();
        immediateContext->PSSetSamplers(0, 1, &samplerPtr);

        if (cubeMesh)
        {
            cubeMesh->BindMeshBuffers(immediateContext);

            for (size_t i = 0; i < cubeMesh->GetNrOfSubMeshes(); ++i)
            {
                cubeMesh->PerformSubMeshDrawCall(immediateContext, i);
            }
        }

        // ----- NYTT: GEOMETRY-PASS TILL G-BUFFER -----
        {
            // Bind G-buffer-RTVs + samma depth-buffer
            gbuffer.SetAsRenderTargets(immediateContext, myDSV);

            float gClear[4] = { 0.f, 0.f, 0.f, 0.f };
            gbuffer.Clear(immediateContext, gClear);

            immediateContext->RSSetViewports(1, &viewport);

            // Samma shaders och buffers som för kuben ovan
            immediateContext->IASetInputLayout(inputLayout.GetInputLayout());
            immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            immediateContext->VSSetShader(vShader, nullptr, 0);
            immediateContext->PSSetShader(pShader, nullptr, 0);

            immediateContext->VSSetConstantBuffers(0, 1, &cb0);
            immediateContext->PSSetShaderResources(0, 1, &textureView);
            immediateContext->PSSetSamplers(0, 1, &samplerPtr);

            if (cubeMesh)
            {
                cubeMesh->BindMeshBuffers(immediateContext);

                for (size_t i = 0; i < cubeMesh->GetNrOfSubMeshes(); ++i)
                {
                    cubeMesh->PerformSubMeshDrawCall(immediateContext, i);
                }
            }
        }

        // Kopiera offscreen-renderingen (sceneRT) till backbuffer som tidigare
        ID3D11Texture2D* backBufferTex = nullptr;
        HRESULT hr = swapChain->GetBuffer(
            0,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(&backBufferTex)
        );

        if (SUCCEEDED(hr) && backBufferTex)
        {
            ID3D11Texture2D* src = sceneRT.GetTexture();
            if (src)
            {
                immediateContext->CopyResource(backBufferTex, src);
            }
            backBufferTex->Release();
        }

        swapChain->Present(0, 0);
    }

    // Manual cleanup
    textureView->Release();
    texture->Release();
    pShader->Release();
    vShader->Release();
    rtv->Release();
    swapChain->Release();
    immediateContext->Release();
    device->Release();

    return 0;
}
