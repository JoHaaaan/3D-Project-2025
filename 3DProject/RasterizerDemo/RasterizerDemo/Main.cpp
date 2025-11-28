#include <Windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <vector>                 // <--- NYTT
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
#include "GBufferD3D11.h"

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
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
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
    ID3D11RenderTargetView* rtv = nullptr;  // backbuffer RTV (används bara ev. senare)
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

    // --- COMPUTE-relaterade pekare ---
    ID3D11ComputeShader* lightingCS = nullptr;
    ID3D11Texture2D* lightingTex = nullptr;
    ID3D11UnorderedAccessView* lightingUAV = nullptr;

    SetupD3D11(WIDTH, HEIGHT, window, device, immediateContext, swapChain, rtv, viewport);

    std::string vShaderByteCode;
    SetupPipeline(device, vertexBuffer, vShader, pShader, vShaderByteCode);

    // Depth buffer wrapper
    DepthBufferD3D11 depthBuffer(device, WIDTH, HEIGHT, false);

    // Offscreen render target (forward-rendering, används mest historiskt nu)
    RenderTargetD3D11 sceneRT;
    sceneRT.Initialize(device, WIDTH, HEIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, false);

    // G-buffer-instans
    GBufferD3D11 gbuffer;
    gbuffer.Initialize(device, WIDTH, HEIGHT);

    // --- CREATE compute output lightingTex + lightingUAV ---
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = WIDTH;
        desc.Height = HEIGHT;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        HRESULT hr = device->CreateTexture2D(&desc, nullptr, &lightingTex);
        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create lightingTex\n");
        }

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = desc.Format;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;

        hr = device->CreateUnorderedAccessView(lightingTex, &uavDesc, &lightingUAV);
        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create lightingUAV\n");
        }
    }

    // --- LOAD compute shader LightingCS.cso ---
    {
        std::ifstream reader("LightingCS.cso", std::ios::binary | std::ios::ate);
        if (reader)
        {
            size_t size = static_cast<size_t>(reader.tellg());
            std::vector<char> buffer(size);
            reader.seekg(0);
            reader.read(buffer.data(), size);

            HRESULT hr = device->CreateComputeShader(
                buffer.data(),
                buffer.size(),
                nullptr,
                &lightingCS
            );
            if (FAILED(hr))
            {
                OutputDebugStringA("Failed to create compute shader!\n");
            }
        }
        else
        {
            OutputDebugStringA("Could not open LightingCS.cso\n");
        }
    }

    inputLayout.AddInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT);
    inputLayout.AddInputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT);
    inputLayout.AddInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);
    inputLayout.FinalizeInputLayout(device, vShaderByteCode.data(), vShaderByteCode.size());

    // Triangle for testing (Render-helpern)
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

    // Mesh
    const MeshD3D11* cubeMesh = GetMesh("cube.obj", device);

    // Material buffer
    Material mat{};
    if (cubeMesh && cubeMesh->GetNrOfSubMeshes() > 0)
    {
        const auto& matData = cubeMesh->GetMaterial(0);
        mat.ambient = matData.ambient;
        mat.diffuse = matData.diffuse;
        mat.specular = matData.specular;
        mat.specularPower = matData.specularPower;
    }
    else
    {
        mat = Material{
            XMFLOAT3(0.2f, 0.2f, 0.2f), 0.f,
            XMFLOAT3(0.5f, 0.5f, 0.5f), 0.f,
            XMFLOAT3(0.5f, 0.5f, 0.5f), 100.f
        };
    }
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

    auto updateMaterialBufferForSubMesh = [&](size_t subMeshIndex)
        {
            if (!cubeMesh || subMeshIndex >= cubeMesh->GetNrOfSubMeshes())
                return;

            const auto& matData = cubeMesh->GetMaterial(subMeshIndex);
            Material currentMaterial{
                matData.ambient, 0.f,
                matData.diffuse, 0.f,
                matData.specular, matData.specularPower
            };

            materialBuffer.UpdateBuffer(immediateContext, &currentMaterial);
        };

    auto previousTime = std::chrono::high_resolution_clock::now();
    float rotationAngle = 90.f;

    const float mouseSens = 0.1f;

    // Static cube world matrix - position (5, 0, 0), scale (1, 1, 1)
    XMMATRIX staticCubeWorld = XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(5.0f, 0.0f, 0.0f);

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
        XMVECTOR  objectPos = XMVectorSet(x, 0.f, z, 1.f);
        XMMATRIX  lookAtMatrix = XMMatrixLookAtLH(objectPos, XMVectorZero(), XMVectorSet(0.f, 1.f, 0.f, 0.f));
        XMMATRIX  worldMatrix = XMMatrixTranspose(lookAtMatrix) * translation;

        // Update matrix buffer
        MatrixPair data;
        XMStoreFloat4x4(&data.world, XMMatrixTranspose(worldMatrix));
        XMStoreFloat4x4(&data.viewProj, XMMatrixTranspose(VIEW_PROJ));
        constantBuffer.UpdateBuffer(immediateContext, &data);

        // Camera movement + mouselook
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

        // ----- FORWARD-PASS TILL sceneRT (behalls för nu) -----
        /*Render(immediateContext, sceneRTV, myDSV, viewport,
            vShader, pShader, inputLayout.GetInputLayout(), vertexBuffer.GetBuffer(),
            constantBuffer.GetBuffer(), textureView,
            samplerState.GetSamplerState(), worldMatrix);*/

        // --- CUBE RENDERING (forward, in i sceneRT) ---
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
                updateMaterialBufferForSubMesh(i);
                cubeMesh->PerformSubMeshDrawCall(immediateContext, i);
            }
        }

        // Render static cube (forward)
        {
            MatrixPair staticData;
            XMStoreFloat4x4(&staticData.world, XMMatrixTranspose(staticCubeWorld));
            XMStoreFloat4x4(&staticData.viewProj, XMMatrixTranspose(VIEW_PROJ));
            constantBuffer.UpdateBuffer(immediateContext, &staticData);

            if (cubeMesh)
            {
                cubeMesh->BindMeshBuffers(immediateContext);

                for (size_t i = 0; i < cubeMesh->GetNrOfSubMeshes(); ++i)
                {
                    updateMaterialBufferForSubMesh(i);
                    cubeMesh->PerformSubMeshDrawCall(immediateContext, i);
                }
            }
        }

        // ----- NEW: GEOMETRY PASS TO G-BUFFER -----
        {
            gbuffer.SetAsRenderTargets(immediateContext, myDSV);

            float gClear[4] = { 0.f, 0.f, 0.f, 0.f };
            gbuffer.Clear(immediateContext, gClear);

            // IMPORTANT: clear depth so this pass can write
            immediateContext->ClearDepthStencilView(
                myDSV,
                D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                1.0f,
                0
            );

            immediateContext->RSSetViewports(1, &viewport);

            // same shaders and bindings as now...
            immediateContext->IASetInputLayout(inputLayout.GetInputLayout());
            immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            immediateContext->VSSetShader(vShader, nullptr, 0);
            immediateContext->PSSetShader(pShader, nullptr, 0);

            ID3D11Buffer* cb0 = constantBuffer.GetBuffer();
            immediateContext->VSSetConstantBuffers(0, 1, &cb0);
            immediateContext->PSSetShaderResources(0, 1, &textureView);
            ID3D11SamplerState* samplerPtr = samplerState.GetSamplerState();
            immediateContext->PSSetSamplers(0, 1, &samplerPtr);

            // rotating cube
            {
                MatrixPair rotatingData;
                XMStoreFloat4x4(&rotatingData.world, XMMatrixTranspose(worldMatrix));
                XMStoreFloat4x4(&rotatingData.viewProj, XMMatrixTranspose(VIEW_PROJ));
                constantBuffer.UpdateBuffer(immediateContext, &rotatingData);

                if (cubeMesh)
                {
                    cubeMesh->BindMeshBuffers(immediateContext);
                    for (size_t i = 0; i < cubeMesh->GetNrOfSubMeshes(); ++i)
                    {
                        updateMaterialBufferForSubMesh(i);
                        cubeMesh->PerformSubMeshDrawCall(immediateContext, i);
                    }
                }
            }

            // static cube
            {
                MatrixPair staticData;
                XMStoreFloat4x4(&staticData.world, XMMatrixTranspose(staticCubeWorld));
                XMStoreFloat4x4(&staticData.viewProj, XMMatrixTranspose(VIEW_PROJ));
                constantBuffer.UpdateBuffer(immediateContext, &staticData);

                if (cubeMesh)
                {
                    cubeMesh->BindMeshBuffers(immediateContext);
                    for (size_t i = 0; i < cubeMesh->GetNrOfSubMeshes(); ++i)
                    {
                        updateMaterialBufferForSubMesh(i);
                        cubeMesh->PerformSubMeshDrawCall(immediateContext, i);
                    }
                }
            }
        }


        // ----- LIGHTING-PASS MED COMPUTE SHADER (albedo + debug) -----
        {
            // Debug: kolla att SRVs faktiskt finns
            if (!gbuffer.GetAlbedoSRV())
                OutputDebugStringA("GBuffer Albedo SRV is NULL!\n");
            if (!gbuffer.GetNormalSRV())
                OutputDebugStringA("GBuffer Normal SRV is NULL!\n");
            if (!gbuffer.GetSpecSRV())
                OutputDebugStringA("GBuffer Spec SRV is NULL!\n");

            // Viktigt: unbinda render targets innan vi läser dem som SRV i compute
            {
                ID3D11RenderTargetView* nullRTVs[3] = { nullptr, nullptr, nullptr };
                immediateContext->OMSetRenderTargets(3, nullRTVs, nullptr);
            }

            // 1) Bind G-buffer-SRVs till compute: t0, t1, t2
            ID3D11ShaderResourceView* srvs[3] =
            {
                gbuffer.GetAlbedoSRV(), // t0
                gbuffer.GetNormalSRV(), // t1
                gbuffer.GetSpecSRV()    // t2 (just nu "extra", vi använder bara albedo i shadern)
            };
            immediateContext->CSSetShaderResources(0, 3, srvs);

            // 2) Bind ljus/material/kamera-constant buffers till b1,b2,b3
            ID3D11Buffer* csCBs[3] = { lightCB, materialCB, cameraCB };
            immediateContext->CSSetConstantBuffers(1, 3, csCBs);
            // startslot = 1  b1,b2,b3 (matchar LightingCS.hlsl)

            // 3) Bind UAV som output
            ID3D11UnorderedAccessView* uavs[1] = { lightingUAV };
            UINT initialCounts[1] = { 0 };
            immediateContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

            // 4) Sätt compute shader och kör
            immediateContext->CSSetShader(lightingCS, nullptr, 0);

            UINT groupsX = (WIDTH + 15) / 16;
            UINT groupsY = (HEIGHT + 15) / 16;
            immediateContext->Dispatch(groupsX, groupsY, 1);

            // 5) Städa upp bindings
            ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
            immediateContext->CSSetShaderResources(0, 3, nullSRVs);

            ID3D11UnorderedAccessView* nullUAVs[1] = { nullptr };
            UINT zeros[1] = { 0 };
            immediateContext->CSSetUnorderedAccessViews(0, 1, nullUAVs, zeros);

            immediateContext->CSSetShader(nullptr, nullptr, 0);
        }



        // ----- KOPIERA compute-resultat (lightingTex) TILL BACKBUFFER -----
        ID3D11Texture2D* backBufferTex = nullptr;
        HRESULT hr = swapChain->GetBuffer(
            0,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(&backBufferTex)
        );

        if (SUCCEEDED(hr) && backBufferTex)
        {
            ID3D11Texture2D* src = lightingTex;  // <-- VIKTIGT anvanda compute-output
            if (src)
            {
                immediateContext->CopyResource(backBufferTex, src);
            }
            backBufferTex->Release();
        }

        swapChain->Present(0, 0);
    }

    // Manual cleanup
    if (lightingUAV)  lightingUAV->Release();
    if (lightingTex)  lightingTex->Release();
    if (lightingCS)   lightingCS->Release();

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
