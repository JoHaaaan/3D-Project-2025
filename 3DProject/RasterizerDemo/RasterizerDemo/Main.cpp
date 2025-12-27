#include <Windows.h>
#include <chrono>
#include <vector>
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
#include "OBJParser.h"
#include "MeshD3D11.h"
#include "GBufferD3D11.h"
#include "GameObject.h"
#include "ShadowMapD3D11.h"
#include "ShaderLoader.h"
#include "TextureLoader.h"
#include "LightManager.h"
#include "EnvironmentMapRenderer.h"

using namespace DirectX;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Transformation parameters
static const float FOV = XMConvertToRadians(45.0f);
static const float ASPECT_RATIO = 1280.0f / 720.0f;
static const float NEAR_PLANE = 0.1f;
static const float FAR_PLANE = 100.0f;

// Global VIEW_PROJ matrix (required by RenderHelper)
XMMATRIX VIEW_PROJ;

struct Material
{
    XMFLOAT3 ambient;
    float    padding1;
    XMFLOAT3 diffuse;
    float    padding2;
    XMFLOAT3 specular;
    float    specularPower;
};

struct LightingToggles
{
    int showAlbedoOnly;
    int enableDiffuse;
    int enableSpecular;
    int padding;
};

// Helper to create rasterizer states
void CreateRasterizerStates(ID3D11Device* device,
    ID3D11RasterizerState*& solidState,
    ID3D11RasterizerState*& wireframeState)
{
    D3D11_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.CullMode = D3D11_CULL_BACK;
    rastDesc.FrontCounterClockwise = FALSE;
    rastDesc.DepthClipEnable = TRUE;

    device->CreateRasterizerState(&rastDesc, &solidState);

    rastDesc.FillMode = D3D11_FILL_WIREFRAME;
    device->CreateRasterizerState(&rastDesc, &wireframeState);
}

// Helper to create shadow sampler
ID3D11SamplerState* CreateShadowSampler(ID3D11Device* device)
{
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;
    desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;

    ID3D11SamplerState* sampler = nullptr;
    device->CreateSamplerState(&desc, &sampler);
    return sampler;
}

// Helper to create compute output resources
void CreateComputeOutputResources(ID3D11Device* device, UINT width, UINT height,
    ID3D11Texture2D*& texture, ID3D11UnorderedAccessView*& uav)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

    device->CreateTexture2D(&desc, nullptr, &texture);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = desc.Format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(texture, &uavDesc, &uav);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    const UINT WIDTH = 1024;
    const UINT HEIGHT = 576;

    // Window setup
    HWND window;
    SetupWindow(hInstance, WIDTH, HEIGHT, nCmdShow, window);

    POINT center = { WIDTH / 2, HEIGHT / 2 };
    ClientToScreen(window, &center);
    ShowCursor(FALSE);
    SetCursorPos(center.x, center.y);

    // D3D11 core objects
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    D3D11_VIEWPORT viewport;

    SetupD3D11(WIDTH, HEIGHT, window, device, context, swapChain, rtv, viewport);

    // Rasterizer states
    ID3D11RasterizerState* solidRasterizerState = nullptr;
    ID3D11RasterizerState* wireframeRasterizerState = nullptr;
    CreateRasterizerStates(device, solidRasterizerState, wireframeRasterizerState);

    // Shaders
    ID3D11VertexShader* vShader = nullptr;
    ID3D11PixelShader* pShader = nullptr;
    std::string vShaderByteCode;
    VertexBufferD3D11 unusedVertexBuffer;
    SetupPipeline(device, unusedVertexBuffer, vShader, pShader, vShaderByteCode);

    // Tessellation shaders
    ID3D11VertexShader* tessVS = ShaderLoader::CreateVertexShader(device, "TessellationVS.cso");
    ID3D11HullShader* tessHS = ShaderLoader::CreateHullShader(device, "TessellationHS.cso");
    ID3D11DomainShader* tessDS = ShaderLoader::CreateDomainShader(device, "TessellationDS.cso");

    // Environment mapping shaders
    ID3D11PixelShader* reflectionPS = ShaderLoader::CreatePixelShader(device, "ReflectionPS.cso");
    ID3D11PixelShader* cubeMapPS = ShaderLoader::CreatePixelShader(device, "CubeMapPS.cso");

    // Compute shader
    ID3D11ComputeShader* lightingCS = ShaderLoader::CreateComputeShader(device, "LightingCS.cso");

    // Input layout
    InputLayoutD3D11 inputLayout;
    inputLayout.AddInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT);
    inputLayout.AddInputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT);
    inputLayout.AddInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);
    inputLayout.FinalizeInputLayout(device, vShaderByteCode.data(), vShaderByteCode.size());

    // Buffers
    DepthBufferD3D11 depthBuffer(device, WIDTH, HEIGHT, false);
    GBufferD3D11 gbuffer;
    gbuffer.Initialize(device, WIDTH, HEIGHT);
    ConstantBufferD3D11 constantBuffer(device, sizeof(MatrixPair));
    ConstantBufferD3D11 materialBuffer;
    ConstantBufferD3D11 lightingToggleCB;

    // Compute output
    ID3D11Texture2D* lightingTex = nullptr;
    ID3D11UnorderedAccessView* lightingUAV = nullptr;
    CreateComputeOutputResources(device, WIDTH, HEIGHT, lightingTex, lightingUAV);

    // Shadow mapping
    ShadowMapD3D11 shadowMap;
    if (!shadowMap.Initialize(device, 2048, 2048, 4))
    {
        OutputDebugStringA("Failed to initialize Shadow Map!\n");
        return -1;
    }

    // Environment mapping
    EnvironmentMapRenderer envMapRenderer;
    if (!envMapRenderer.Initialize(device, 512))
    {
        OutputDebugStringA("Failed to initialize Environment Map!\n");
        return -1;
    }

    // Light manager
    LightManager lightManager;
    lightManager.InitializeDefaultLights(device);

    // Meshes
    const MeshD3D11* cubeMesh = GetMesh("cube.obj", device);
    const MeshD3D11* pineAppleMesh = GetMesh("pineapple2.obj", device);
    const MeshD3D11* simpleCubeMesh = GetMesh("SimpleCube.obj", device);
    const MeshD3D11* sphereMesh = GetMesh("sphere.obj", device);

    // Material setup
    Material mat = {
        XMFLOAT3(0.2f, 0.2f, 0.2f), 0.f,
        XMFLOAT3(0.5f, 0.5f, 0.5f), 0.f,
        XMFLOAT3(0.5f, 0.5f, 0.5f), 100.f
    };
    if (cubeMesh && cubeMesh->GetNrOfSubMeshes() > 0)
    {
        const auto& matData = cubeMesh->GetMaterial(0);
        mat.ambient = matData.ambient;
        mat.diffuse = matData.diffuse;
        mat.specular = matData.specular;
        mat.specularPower = matData.specularPower;
    }
    materialBuffer.Initialize(device, sizeof(Material), &mat);

    // Lighting toggles
    LightingToggles toggleData = { 0, 1, 1, 0 };
    lightingToggleCB.Initialize(device, sizeof(LightingToggles), &toggleData);

    // Camera
    ProjectionInfo proj{ FOV, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE };
    CameraD3D11 camera;
    camera.Initialize(device, proj, XMFLOAT3(0.0f, 5.0f, -10.0f));
    camera.RotateForward(XMConvertToRadians(-20.0f));

    // Textures
    ID3D11ShaderResourceView* whiteTexView = TextureLoader::CreateWhiteTexture(device);
    ID3D11ShaderResourceView* pineappleTexView = TextureLoader::LoadTexture(device, "image.png");
    if (!pineappleTexView) pineappleTexView = whiteTexView;

    // Samplers
    SamplerD3D11 samplerState;
    samplerState.Initialize(device, D3D11_TEXTURE_ADDRESS_WRAP);
    ID3D11SamplerState* shadowSampler = CreateShadowSampler(device);

    // Image quad for rotating object
    VertexBufferD3D11 imageQuadVB;
    {
        SimpleVertex quadVerts[] = {
            { { -1.0f,  1.0f, 0.0f }, {0,0,-1}, {0.0f, 0.0f} },
            { {  1.0f,  1.0f, 0.0f }, {0,0,-1}, {1.0f, 0.0f} },
            { { -1.0f, -1.0f, 0.0f }, {0,0,-1}, {0.0f, 1.0f} },
            { {  1.0f,  1.0f, 0.0f }, {0,0,-1}, {1.0f, 0.0f} },
            { {  1.0f, -1.0f, 0.0f }, {0,0,-1}, {1.0f, 1.0f} },
            { { -1.0f, -1.0f, 0.0f }, {0,0,-1}, {0.0f, 1.0f} },
        };
        imageQuadVB.Initialize(device, sizeof(SimpleVertex), 6, quadVerts);
    }

    // Game objects
    std::vector<GameObject> gameObjects;
    gameObjects.emplace_back(cubeMesh);
    gameObjects.emplace_back(cubeMesh);
    gameObjects[1].SetWorldMatrix(XMMatrixTranslation(30.0f, 1.0f, 0.0f));
    gameObjects.emplace_back(pineAppleMesh);
    gameObjects[2].SetWorldMatrix(XMMatrixTranslation(0.0f, 0.0f, -14.0f));
    gameObjects.emplace_back(simpleCubeMesh);
    gameObjects[3].SetWorldMatrix(XMMatrixTranslation(-2.0f, 2.0f, 0.0f));
    gameObjects.emplace_back(simpleCubeMesh);
    gameObjects[4].SetWorldMatrix(XMMatrixTranslation(2.0f, 2.0f, 0.0f));
    gameObjects.emplace_back(simpleCubeMesh);
    gameObjects[5].SetWorldMatrix(XMMatrixScaling(5.0f, 0.2f, 5.0f) * XMMatrixTranslation(0.0f, -1.0f, 0.0f));
    gameObjects.emplace_back(sphereMesh);
    gameObjects[6].SetWorldMatrix(XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(2.0f, 3.0f, -3.0f));

    const size_t REFLECTIVE_OBJECT_INDEX = 3;

    // State
    bool tessellationEnabled = false;
    bool wireframeEnabled = false;
    auto previousTime = std::chrono::high_resolution_clock::now();
    float rotationAngle = 90.f;
    const float mouseSens = 0.1f;

    bool key1Prev = false, key2Prev = false, key3Prev = false, key4Prev = false, key5Prev = false;

    // Main loop
    MSG msg = {};
    while (!(GetKeyState(VK_ESCAPE) & 0x8000) && msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Timing
        auto currentTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(currentTime - previousTime).count();
        previousTime = currentTime;

        rotationAngle += XMConvertToRadians(30.f) * dt;
        if (rotationAngle > XM_2PI) rotationAngle -= XM_2PI;

        // Input handling - Toggles
        bool key1Now = (GetAsyncKeyState('1') & 0x8000) != 0;
        bool key2Now = (GetAsyncKeyState('2') & 0x8000) != 0;
        bool key3Now = (GetAsyncKeyState('3') & 0x8000) != 0;
        bool key4Now = (GetAsyncKeyState('4') & 0x8000) != 0;
        bool key5Now = (GetAsyncKeyState('5') & 0x8000) != 0;

        if (key1Now && !key1Prev) { toggleData.showAlbedoOnly = !toggleData.showAlbedoOnly; lightingToggleCB.UpdateBuffer(context, &toggleData); }
        if (key2Now && !key2Prev) { toggleData.enableDiffuse = !toggleData.enableDiffuse; lightingToggleCB.UpdateBuffer(context, &toggleData); }
        if (key3Now && !key3Prev) { toggleData.enableSpecular = !toggleData.enableSpecular; lightingToggleCB.UpdateBuffer(context, &toggleData); }
        if (key4Now && !key4Prev) { wireframeEnabled = !wireframeEnabled; }
        if (key5Now && !key5Prev) { tessellationEnabled = !tessellationEnabled; }
        key1Prev = key1Now; key2Prev = key2Now; key3Prev = key3Now; key4Prev = key4Now; key5Prev = key5Now;

        // Camera movement
        const float camSpeed = 3.0f;
        if (GetAsyncKeyState('W') & 0x8000) camera.MoveForward(camSpeed * dt);
        if (GetAsyncKeyState('S') & 0x8000) camera.MoveForward(-camSpeed * dt);
        if (GetAsyncKeyState('A') & 0x8000) camera.MoveRight(-camSpeed * dt);
        if (GetAsyncKeyState('D') & 0x8000) camera.MoveRight(camSpeed * dt);
        if (GetAsyncKeyState('Q') & 0x8000) camera.MoveUp(-camSpeed * dt);
        if (GetAsyncKeyState('E') & 0x8000) camera.MoveUp(camSpeed * dt);

        POINT mouseP;
        GetCursorPos(&mouseP);
        camera.RotateRight(XMConvertToRadians((mouseP.x - center.x) * mouseSens));
        camera.RotateForward(XMConvertToRadians((mouseP.y - center.y) * mouseSens));
        SetCursorPos(center.x, center.y);
        camera.UpdateInternalConstantBuffer(context);

        XMFLOAT4X4 vp = camera.GetViewProjectionMatrix();
        VIEW_PROJ = XMLoadFloat4x4(&vp);

        // Update rotating cube
        float armLength = 0.7f;
        float x = armLength * cosf(rotationAngle);
        float z = armLength * sinf(rotationAngle);
        XMVECTOR objectPos = XMVectorSet(x, 0.f, z, 1.f);
        XMMATRIX lookAtMatrix = XMMatrixLookAtLH(objectPos, XMVectorZero(), XMVectorSet(0.f, 1.f, 0.f, 0.f));
        gameObjects[0].SetWorldMatrix(XMMatrixTranspose(lookAtMatrix) * XMMatrixTranslation(x, 0.f, z));

        ID3D11DepthStencilView* myDSV = depthBuffer.GetDSV(0);
        ID3D11Buffer* cb0 = constantBuffer.GetBuffer();
        ID3D11Buffer* cameraCB = camera.GetConstantBuffer();

        // ----- SHADOW PASS -----
        {
            context->IASetInputLayout(inputLayout.GetInputLayout());
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context->VSSetShader(vShader, nullptr, 0);
            context->HSSetShader(nullptr, nullptr, 0);
            context->DSSetShader(nullptr, nullptr, 0);
            context->PSSetShader(nullptr, nullptr, 0);
            context->VSSetConstantBuffers(0, 1, &cb0);

            auto& lights = lightManager.GetLights();
            for (size_t lightIdx = 0; lightIdx < lights.size(); ++lightIdx)
            {
                ID3D11DepthStencilView* shadowDSV = shadowMap.GetDSV(static_cast<UINT>(lightIdx));
                context->ClearDepthStencilView(shadowDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

                D3D11_VIEWPORT shadowViewport = shadowMap.GetViewport();
                context->RSSetViewports(1, &shadowViewport);

                ID3D11RenderTargetView* nullRTV = nullptr;
                context->OMSetRenderTargets(1, &nullRTV, shadowDSV);

                XMMATRIX lightVP = lightManager.GetLightViewProj(lightIdx);

                for (auto& obj : gameObjects)
                {
                    MatrixPair shadowData;
                    XMStoreFloat4x4(&shadowData.world, XMMatrixTranspose(obj.GetWorldMatrix()));
                    XMStoreFloat4x4(&shadowData.viewProj, XMMatrixTranspose(lightVP));
                    constantBuffer.UpdateBuffer(context, &shadowData);

                    const MeshD3D11* mesh = obj.GetMesh();
                    if (mesh)
                    {
                        mesh->BindMeshBuffers(context);
                        for (size_t i = 0; i < mesh->GetNrOfSubMeshes(); ++i)
                            mesh->PerformSubMeshDrawCall(context, i);
                    }
                }
            }
        }

        // ----- ENVIRONMENT MAP PASS -----
        if (cubeMapPS)
        {
            XMFLOAT3 reflectivePos;
            XMStoreFloat3(&reflectivePos, gameObjects[REFLECTIVE_OBJECT_INDEX].GetWorldMatrix().r[3]);

            envMapRenderer.RenderEnvironmentMap(
            context, device, reflectivePos, gameObjects, REFLECTIVE_OBJECT_INDEX,
            vShader, cubeMapPS, inputLayout.GetInputLayout(),
            constantBuffer, materialBuffer, samplerState, whiteTexView
            );
        }

        // Restore main camera
        camera.UpdateInternalConstantBuffer(context);
        context->PSSetConstantBuffers(3, 1, &cameraCB);

        // ----- GEOMETRY PASS -----
        {
            gbuffer.SetAsRenderTargets(context, myDSV);
            float gClear[4] = { 0.f, 0.f, 0.f, 0.f };
            gbuffer.Clear(context, gClear);
            context->ClearDepthStencilView(myDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            context->RSSetViewports(1, &viewport);
            context->RSSetState(wireframeEnabled ? wireframeRasterizerState : solidRasterizerState);
            context->IASetInputLayout(inputLayout.GetInputLayout());
            context->VSSetConstantBuffers(0, 1, &cb0);

            ID3D11SamplerState* samplerPtr = samplerState.GetSamplerState();
            context->PSSetSamplers(0, 1, &samplerPtr);

            if (tessellationEnabled && tessVS && tessHS && tessDS)
            {
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
                context->VSSetShader(tessVS, nullptr, 0);
                context->HSSetShader(tessHS, nullptr, 0);
                context->DSSetShader(tessDS, nullptr, 0);
                context->DSSetConstantBuffers(0, 1, &cb0);
                context->HSSetConstantBuffers(3, 1, &cameraCB);
                context->PSSetShader(pShader, nullptr, 0);
            }
            else
            {
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                context->VSSetShader(vShader, nullptr, 0);
                context->HSSetShader(nullptr, nullptr, 0);
                context->DSSetShader(nullptr, nullptr, 0);
                context->PSSetShader(pShader, nullptr, 0);
            }

            // Render image quad
            {
                MatrixPair rotatingData;
                XMStoreFloat4x4(&rotatingData.world, XMMatrixTranspose(gameObjects[0].GetWorldMatrix()));
                XMStoreFloat4x4(&rotatingData.viewProj, XMMatrixTranspose(VIEW_PROJ));
                constantBuffer.UpdateBuffer(context, &rotatingData);

                UINT stride = sizeof(SimpleVertex), offset = 0;
                ID3D11Buffer* quadVB = imageQuadVB.GetBuffer();
                context->IASetVertexBuffers(0, 1, &quadVB, &stride, &offset);
                context->PSSetShaderResources(0, 1, &pineappleTexView);
                context->Draw(6, 0);

                ID3D11ShaderResourceView* nullSRV = nullptr;
                context->PSSetShaderResources(0, 1, &nullSRV);
            }

            // Render game objects
            for (size_t objIdx = 0; objIdx < gameObjects.size(); ++objIdx)
            {
                if (objIdx == REFLECTIVE_OBJECT_INDEX && reflectionPS)
                {
                    context->PSSetShader(reflectionPS, nullptr, 0);
                    ID3D11ShaderResourceView* envSRV = envMapRenderer.GetEnvironmentSRV();
                    context->PSSetShaderResources(0, 1, &envSRV);
                    context->PSSetSamplers(0, 1, &samplerPtr);

                    gameObjects[objIdx].Draw(context, constantBuffer, materialBuffer, VIEW_PROJ, whiteTexView);

                    ID3D11ShaderResourceView* nullSRV = nullptr;
                    context->PSSetShaderResources(0, 1, &nullSRV);
                    context->PSSetShader(pShader, nullptr, 0);
                }
                else
                {
                    gameObjects[objIdx].Draw(context, constantBuffer, materialBuffer, VIEW_PROJ, whiteTexView);
                }
            }
        }

        // ----- LIGHTING PASS (COMPUTE) -----
        if (lightingCS)
        {
            ID3D11RenderTargetView* nullRTVs[3] = { nullptr, nullptr, nullptr };
            context->OMSetRenderTargets(3, nullRTVs, nullptr);

            ID3D11ShaderResourceView* srvs[3] = { gbuffer.GetAlbedoSRV(), gbuffer.GetNormalSRV(), gbuffer.GetPositionSRV() };
            context->CSSetShaderResources(0, 3, srvs);

            ID3D11ShaderResourceView* shadowSRV = shadowMap.GetSRV();
            context->CSSetShaderResources(3, 1, &shadowSRV);

            ID3D11ShaderResourceView* lightSRV = lightManager.GetLightBufferSRV();
            context->CSSetShaderResources(4, 1, &lightSRV);

            context->CSSetConstantBuffers(2, 1, &cameraCB);
            ID3D11Buffer* toggleCBBuf = lightingToggleCB.GetBuffer();
            context->CSSetConstantBuffers(4, 1, &toggleCBBuf);
            context->CSSetSamplers(1, 1, &shadowSampler);
            context->CSSetUnorderedAccessViews(0, 1, &lightingUAV, nullptr);
            context->CSSetShader(lightingCS, nullptr, 0);
            context->Dispatch((WIDTH + 15) / 16, (HEIGHT + 15) / 16, 1);

            // Cleanup
            ID3D11ShaderResourceView* nullSRVs[5] = { nullptr };
            context->CSSetShaderResources(0, 5, nullSRVs);
            ID3D11UnorderedAccessView* nullUAV = nullptr;
            context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
            context->CSSetShader(nullptr, nullptr, 0);
        }

        // Copy to backbuffer
        ID3D11Texture2D* backBuffer = nullptr;
        if (SUCCEEDED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer)))
        {
            context->CopyResource(backBuffer, lightingTex);
            backBuffer->Release();
        }

        swapChain->Present(0, 0);
    }

    // Cleanup
    if (cubeMapPS) cubeMapPS->Release();
    if (reflectionPS) reflectionPS->Release();
    if (tessVS) tessVS->Release();
    if (tessHS) tessHS->Release();
    if (tessDS) tessDS->Release();
    if (lightingUAV) lightingUAV->Release();
    if (lightingTex) lightingTex->Release();
    if (lightingCS) lightingCS->Release();
    if (shadowSampler) shadowSampler->Release();
    if (solidRasterizerState) solidRasterizerState->Release();
    if (wireframeRasterizerState) wireframeRasterizerState->Release();
    if (pineappleTexView && pineappleTexView != whiteTexView) pineappleTexView->Release();
    if (whiteTexView) whiteTexView->Release();
    if (pShader) pShader->Release();
    if (vShader) vShader->Release();
    if (rtv) rtv->Release();
    swapChain->Release();
    context->Release();
    device->Release();
    UnloadMeshes();

    return 0;
}
