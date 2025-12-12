#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")


#include <Windows.h>
#include <iostream>
#include <fstream>
#include <string>
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
#include "RenderTargetD3D11.h"
#include "OBJParser.h"
#include "MeshD3D11.h"
#include "GBufferD3D11.h"
#include "GameObject.h"
#include "ShadowMapD3D11.h"
#include "StructuredBufferD3D11.h"
#include "SpotLightCollectionD3D11.h"

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
    int showAlbedoOnly; // 1 = show only albedo
    int enableDiffuse;  // 2 = enable diffuse
    int enableSpecular; // 3 = enable specular
    int padding;        // unused, for 16-byte alignment
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    const UINT WIDTH = 1024;
    const UINT HEIGHT = 576;
    HWND window;
    SetupWindow(hInstance, WIDTH, HEIGHT, nCmdShow, window);

    // Center mouse
    POINT center = { WIDTH / 2, HEIGHT / 2 };
    ClientToScreen(window, &center);
    ShowCursor(FALSE);
    SetCursorPos(center.x, center.y);

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* immediateContext = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    D3D11_VIEWPORT viewport;
    ID3D11VertexShader* vShader = nullptr;
    ID3D11PixelShader* pShader = nullptr;
    InputLayoutD3D11 inputLayout;
    VertexBufferD3D11 vertexBuffer;
    ConstantBufferD3D11 constantBuffer;
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* textureView = nullptr;
    SamplerD3D11 samplerState;
    ConstantBufferD3D11 materialBuffer;
    ConstantBufferD3D11 lightingToggleCB;
    CameraD3D11 camera;

    // Compute related
    ID3D11ComputeShader* lightingCS = nullptr;
    ID3D11Texture2D* lightingTex = nullptr;
    ID3D11UnorderedAccessView* lightingUAV = nullptr;

    // Tessellation shaders
    ID3D11VertexShader* tessVS = nullptr;
    ID3D11HullShader* tessHS = nullptr;
    ID3D11DomainShader* tessDS = nullptr;
    std::string tessVSByteCode;
    bool tessellationEnabled = false;

    // Rasterizer states for wireframe
    ID3D11RasterizerState* solidRasterizerState = nullptr;
    ID3D11RasterizerState* wireframeRasterizerState = nullptr;
    bool wireframeEnabled = false;

    SetupD3D11(WIDTH, HEIGHT, window, device, immediateContext, swapChain, rtv, viewport);

    // Create rasterizer states
    {
        D3D11_RASTERIZER_DESC rastDesc = {};
        rastDesc.FillMode = D3D11_FILL_SOLID;
        rastDesc.CullMode = D3D11_CULL_BACK;
        rastDesc.FrontCounterClockwise = FALSE;
        rastDesc.DepthClipEnable = TRUE;
        
        HRESULT hr = device->CreateRasterizerState(&rastDesc, &solidRasterizerState);
        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create solid rasterizer state!\n");
        }

        rastDesc.FillMode = D3D11_FILL_WIREFRAME;
        hr = device->CreateRasterizerState(&rastDesc, &wireframeRasterizerState);
        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create wireframe rasterizer state!\n");
        }
        else
        {
            OutputDebugStringA("Wireframe rasterizer state created successfully!\n");
        }
    }

    std::string vShaderByteCode;
    SetupPipeline(device, vertexBuffer, vShader, pShader, vShaderByteCode);

    // Load tessellation shaders
    {
        auto loadShader = [](const std::string& filename) -> std::vector<char>
        {
            std::ifstream reader(filename, std::ios::binary | std::ios::ate);
            if (!reader)
            {
                return {};
            }
            size_t size = static_cast<size_t>(reader.tellg());
            std::vector<char> buffer(size);
            reader.seekg(0);
            reader.read(buffer.data(), size);
            return buffer;
        };

        // Load Tessellation Vertex Shader
        auto tessVSData = loadShader("TessellationVS.cso");
        if (!tessVSData.empty())
        {
            HRESULT hr = device->CreateVertexShader(tessVSData.data(), tessVSData.size(), nullptr, &tessVS);
            if (SUCCEEDED(hr))
            {
                tessVSByteCode.assign(tessVSData.begin(), tessVSData.end());
                OutputDebugStringA("Tessellation VS loaded successfully!\n");
            }
            else
            {
                OutputDebugStringA("Failed to create tessellation VS!\n");
            }
        }
        else
        {
            OutputDebugStringA("TessellationVS.cso not found!\n");
        }

        // Load Tessellation Hull Shader
        auto tessHSData = loadShader("TessellationHS.cso");
        if (!tessHSData.empty())
        {
            HRESULT hr = device->CreateHullShader(tessHSData.data(), tessHSData.size(), nullptr, &tessHS);
            if (SUCCEEDED(hr))
            {
                OutputDebugStringA("Tessellation HS loaded successfully!\n");
            }
            else
            {
                OutputDebugStringA("Failed to create tessellation HS!\n");
            }
        }
        else
        {
            OutputDebugStringA("TessellationHS.cso not found!\n");
        }

        // Load Tessellation Domain Shader
        auto tessDSData = loadShader("TessellationDS.cso");
        if (!tessDSData.empty())
        {
            HRESULT hr = device->CreateDomainShader(tessDSData.data(), tessDSData.size(), nullptr, &tessDS);
            if (SUCCEEDED(hr))
            {
                OutputDebugStringA("Tessellation DS loaded successfully!\n");
            }
            else
            {
                OutputDebugStringA("Failed to create tessellation DS!\n");
            }
        }
        else
        {
            OutputDebugStringA("TessellationDS.cso not found!\n");
        }
    }

    // Depth buffer
    DepthBufferD3D11 depthBuffer(device, WIDTH, HEIGHT, false);

    // G-buffer
    GBufferD3D11 gbuffer;
    gbuffer.Initialize(device, WIDTH, HEIGHT);

    // --- SHADOW MAPPING SETUP ---

    // 1. Create Shadow Map Resources
    ShadowMapD3D11 shadowMap;
    // 2048x2048 resolution, 4 slices (for 4 lights)
    if (!shadowMap.Initialize(device, 2048, 2048, 4))
    {
        OutputDebugStringA("Failed to initialize Shadow Map!\n");
        return -1;
    }

    // 2. Define Lights (At least 4 required: 1 Directional + 1 Spot minimum)
    std::vector<LightData> lights;
    lights.resize(4);

    // Light 0: Directional Light (The Sun)
    {
        lights[0].type = 0; // Directional
        lights[0].enabled = 1;
        lights[0].color = XMFLOAT3(1.0f, 0.9f, 0.8f);
        lights[0].intensity = 2.0f; // Increased from 1.0
        lights[0].position = XMFLOAT3(20.0f, 30.0f, -20.0f); // Position only needed for shadow matrix
        lights[0].direction = XMFLOAT3(-1.0f, -1.0f, 1.0f); // Pointing down-inward

        // View Matrix (Look at origin)
        XMVECTOR lightPos = XMLoadFloat3(&lights[0].position);
        XMVECTOR lightDir = XMLoadFloat3(&lights[0].direction);
        XMMATRIX view = XMMatrixLookToLH(lightPos, lightDir, XMVectorSet(0, 1, 0, 0));

        // Projection Matrix (Orthographic for Directional Lights)
        // Width/Height covers the scene size (e.g., 50x50 units)
        XMMATRIX proj = XMMatrixOrthographicLH(60.0f, 60.0f, 1.0f, 100.0f);

        // Store Transposed ViewProj
        XMStoreFloat4x4(&lights[0].viewProj, XMMatrixTranspose(view * proj));
    }

    // Light 1: Spot Light (Green)
    {
        lights[1].type = 1; // Spot
        lights[1].enabled = 1;
        lights[1].color = XMFLOAT3(0.0f, 1.0f, 0.0f);
        lights[1].intensity = 10.0f; // Increased from 2.0
        lights[1].position = XMFLOAT3(0.0f, 10.0f, 0.0f); // Above center
        lights[1].direction = XMFLOAT3(0.0f, -1.0f, 0.0f); // Pointing down
        lights[1].range = 50.0f; // Increased from 30.0
        lights[1].spotAngle = XMConvertToRadians(60.0f); // Wider cone

        // View Matrix
        XMVECTOR lightPos = XMLoadFloat3(&lights[1].position);
        XMVECTOR lightDir = XMLoadFloat3(&lights[1].direction);
        XMMATRIX view = XMMatrixLookToLH(lightPos, lightDir, XMVectorSet(1, 0, 0, 0)); // Up vector X if looking down Y

        // Projection Matrix (Perspective for Spot Lights)
        XMMATRIX proj = XMMatrixPerspectiveFovLH(lights[1].spotAngle, 1.0f, 0.5f, 50.0f);

        XMStoreFloat4x4(&lights[1].viewProj, XMMatrixTranspose(view * proj));
    }

    // Light 2: Spot Light (Red)
    {
        lights[2] = lights[1]; // Copy settings
        lights[2].color = XMFLOAT3(1.0f, 0.0f, 0.0f);
        lights[2].position = XMFLOAT3(-10.0f, 5.0f, -5.0f);
        lights[2].direction = XMFLOAT3(1.0f, -0.5f, 1.0f);

        XMVECTOR lightPos = XMLoadFloat3(&lights[2].position);
        XMVECTOR lightDir = XMLoadFloat3(&lights[2].direction);
        XMMATRIX view = XMMatrixLookToLH(lightPos, lightDir, XMVectorSet(0, 1, 0, 0));
        XMMATRIX proj = XMMatrixPerspectiveFovLH(lights[2].spotAngle, 1.0f, 0.5f, 50.0f);
        XMStoreFloat4x4(&lights[2].viewProj, XMMatrixTranspose(view * proj));
    }

    // Light 3: Spot Light (Blue)
    {
        lights[3] = lights[1]; // Copy settings
        lights[3].color = XMFLOAT3(0.0f, 0.0f, 1.0f);
        lights[3].position = XMFLOAT3(10.0f, 5.0f, -5.0f);
        lights[3].direction = XMFLOAT3(-1.0f, -0.5f, 1.0f);

        XMVECTOR lightPos = XMLoadFloat3(&lights[3].position);
        XMVECTOR lightDir = XMLoadFloat3(&lights[3].direction);
        XMMATRIX view = XMMatrixLookToLH(lightPos, lightDir, XMVectorSet(0, 1, 0, 0));
        XMMATRIX proj = XMMatrixPerspectiveFovLH(lights[3].spotAngle, 1.0f, 0.5f, 50.0f);
        XMStoreFloat4x4(&lights[3].viewProj, XMMatrixTranspose(view * proj));
    }

    // 3. Create Structured Buffer for Lights
    StructuredBufferD3D11 lightStructuredBuffer;
    lightStructuredBuffer.Initialize(device, sizeof(LightData), (UINT)lights.size(), lights.data());
    
    OutputDebugStringA("Light Structured Buffer created with 4 lights:\n");
    char debugBuf[256];
    for (size_t i = 0; i < lights.size(); ++i)
    {
        sprintf_s(debugBuf, "  Light %zu: Type=%d, Enabled=%d, Color=(%.2f,%.2f,%.2f), Intensity=%.2f\n",
            i, lights[i].type, lights[i].enabled, 
            lights[i].color.x, lights[i].color.y, lights[i].color.z,
            lights[i].intensity);
        OutputDebugStringA(debugBuf);
    }

    // Create compute output texture + UAV
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

    // Load compute shader LightingCS.cso
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
            else
            {
                OutputDebugStringA("Successfully loaded LightingCS.cso and created compute shader!\n");
            }
        }
        else
        {
            OutputDebugStringA("Could not open LightingCS.cso - FILE NOT FOUND!\n");
        }
    }

    inputLayout.AddInputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT);
    inputLayout.AddInputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT);
    inputLayout.AddInputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);
    inputLayout.FinalizeInputLayout(device, vShaderByteCode.data(), vShaderByteCode.size());

    // Test triangle (not really used now but ok to keep)
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

    // Rotating image quad (6 vertices forming 2 triangles)
    VertexBufferD3D11 imageQuadVB;
    {
        // Create a quad facing the camera (Z = 0), centered at origin
        // Scale it to a reasonable size (2x2 units)
        SimpleVertex quadVerts[] = {
            // First triangle
            { { -1.0f,  1.0f, 0.0f }, {0,0,-1}, {0.0f, 0.0f} }, // Top-left
            { {  1.0f,  1.0f, 0.0f }, {0,0,-1}, {1.0f, 0.0f} }, // Top-right
            { { -1.0f, -1.0f, 0.0f }, {0,0,-1}, {0.0f, 1.0f} }, // Bottom-left

            // Second triangle
            { {  1.0f,  1.0f, 0.0f }, {0,0,-1}, {1.0f, 0.0f} }, // Top-right
            { {  1.0f, -1.0f, 0.0f }, {0,0,-1}, {1.0f, 1.0f} }, // Bottom-right
            { { -1.0f, -1.0f, 0.0f }, {0,0,-1}, {0.0f, 1.0f} }, // Bottom-left
        };
        imageQuadVB.Initialize(
            device,
            sizeof(SimpleVertex),
            static_cast<UINT>(sizeof(quadVerts) / sizeof(quadVerts[0])),
            quadVerts);
    }

    // Matrix constant buffer
    constantBuffer.Initialize(device, sizeof(MatrixPair));


    // Mesh
    const MeshD3D11* cubeMesh = GetMesh("cube.obj", device);
    const MeshD3D11* pineAppleMesh = GetMesh("pineapple2.obj", device);
    const MeshD3D11* simpleCubeMesh = GetMesh("SimpleCube.obj", device);

    // Material buffer (still used in geometry pass / PS)
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

    // Lighting toggles buffer
    LightingToggles toggleData = {};
    toggleData.showAlbedoOnly = 0;
    toggleData.enableDiffuse = 1;
    toggleData.enableSpecular = 1;
    toggleData.padding = 0;
    lightingToggleCB.Initialize(device, sizeof(LightingToggles), &toggleData);

    // Camera setup
    ProjectionInfo proj{ FOV, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE };
    camera.Initialize(device, proj, XMFLOAT3(0.0f, 5.0f, -10.0f));
    
    // Rotate camera to look at origin
    camera.RotateForward(XMConvertToRadians(-20.0f));

    // Create a white 1x1 fallback texture (used when material has no map_Kd)
    ID3D11Texture2D* whiteTex = nullptr;
    ID3D11ShaderResourceView* whiteTexView = nullptr;
    {
        unsigned char whitePixel[4] = { 255, 255, 255, 255 };

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA texData = {};
        texData.pSysMem = whitePixel;
        texData.SysMemPitch = 4;

        device->CreateTexture2D(&texDesc, &texData, &whiteTex);
        device->CreateShaderResourceView(whiteTex, nullptr, &whiteTexView);
        if (whiteTex) whiteTex->Release(); // SRV holds reference
    }

    // Texture loading (fallback texture used when material has no map_Kd)
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

    // Load pineapple texture manually for testing
    ID3D11Texture2D* pineappleTex = nullptr;
    ID3D11ShaderResourceView* pineappleTexView = nullptr;
    {
        int wTex, hTex, channels;
        unsigned char* imageData = stbi_load("image.png", &wTex, &hTex, &channels, 4);
        
        if (imageData)
        {
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

            device->CreateTexture2D(&texDesc, &texData, &pineappleTex);
            stbi_image_free(imageData);
            device->CreateShaderResourceView(pineappleTex, nullptr, &pineappleTexView);
            if (pineappleTex) pineappleTex->Release();
        
            OutputDebugStringA("Successfully loaded pineapple2.jpeg!\n");
        }
        else
        {
            OutputDebugStringA("Failed to load pineapple2.jpeg!\n");
            pineappleTexView = whiteTexView; // fallback to white
        }
    }

    samplerState.Initialize(device, D3D11_TEXTURE_ADDRESS_WRAP);

    // Create shadow comparison sampler for PCF
    ID3D11SamplerState* shadowSampler = nullptr;
    {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        desc.BorderColor[0] = 1.0f; // Outside shadow = fully lit
        desc.BorderColor[1] = 1.0f;
        desc.BorderColor[2] = 1.0f;
        desc.BorderColor[3] = 1.0f;
        desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        
        HRESULT hr = device->CreateSamplerState(&desc, &shadowSampler);
        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create shadow sampler!\n");
        }
        else
        {
            OutputDebugStringA("Shadow sampler created successfully!\n");
        }
    }

    ID3D11Buffer* materialCB = materialBuffer.GetBuffer();
    ID3D11Buffer* cameraCB = camera.GetConstantBuffer();

    // Bind material and camera to PS for geometry pass
    immediateContext->PSSetConstantBuffers(2, 1, &materialCB);
    immediateContext->PSSetConstantBuffers(3, 1, &cameraCB);

    // NOTE: we no longer bind a single global albedo SRV here.
    // Geometry pass will bind per-submesh diffuse SRV (from OBJ .mtl map_Kd) if present,
    // otherwise fall back to the textureView loaded above.

    auto updateMaterialBufferForSubMesh = [&](const MeshD3D11* mesh, size_t subMeshIndex)
        {
            if (!mesh || subMeshIndex >= mesh->GetNrOfSubMeshes())
         return;

            const auto& matData = mesh->GetMaterial(subMeshIndex);
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

    // Create GameObjects
    std::vector<GameObject> gameObjects;
    
    // Rotating cube with image quad
    gameObjects.emplace_back(cubeMesh);
    
  // Static cube  
    gameObjects.emplace_back(cubeMesh);
    gameObjects[1].SetWorldMatrix(XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(30.0f, 1.0f, 0.0f));
    
    // Pineapple
    gameObjects.emplace_back(pineAppleMesh);
    gameObjects[2].SetWorldMatrix(XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, 0.0f, -14.0f));
    
    // SimpleCube 1
    gameObjects.emplace_back(simpleCubeMesh);
    gameObjects[3].SetWorldMatrix(XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(-2.0f, 2.0f, 0.0f));
    
    // SimpleCube 2 (NEW - next to SimpleCube 1)
    gameObjects.emplace_back(simpleCubeMesh);
    gameObjects[4].SetWorldMatrix(XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(2.0f, 2.0f, 0.0f));
    
    // SimpleCube 3 (NEW - on the ground to catch shadows)
    gameObjects.emplace_back(simpleCubeMesh);
    gameObjects[5].SetWorldMatrix(XMMatrixScaling(5.0f, 0.2f, 5.0f) * XMMatrixTranslation(0.0f, -1.0f, 0.0f)); // Flat ground plane

    // Toggle key state
    static bool key1Prev = false;
    static bool key2Prev = false;
    static bool key3Prev = false;
    static bool key4Prev = false;
    static bool key5Prev = false; // For tessellation toggle

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

        // --- Toggle handling (1,2,3,4,5) ---
        short k1 = GetAsyncKeyState('1');
        short k2 = GetAsyncKeyState('2');
        short k3 = GetAsyncKeyState('3');
        short k4 = GetAsyncKeyState('4');
        short k5 = GetAsyncKeyState('5');

        bool key1Now = (k1 & 0x8000) != 0;
        bool key2Now = (k2 & 0x8000) != 0;
        bool key3Now = (k3 & 0x8000) != 0;
        bool key4Now = (k4 & 0x8000) != 0;
        bool key5Now = (k5 & 0x8000) != 0;

        if (key1Now && !key1Prev)
        {
            toggleData.showAlbedoOnly = !toggleData.showAlbedoOnly;
            lightingToggleCB.UpdateBuffer(immediateContext, &toggleData);
            OutputDebugStringA(toggleData.showAlbedoOnly ? "ShowAlbedoOnly: ON\n" : "ShowAlbedoOnly: OFF\n");
        }
        key1Prev = key1Now;

        if (key2Now && !key2Prev)
        {
            toggleData.enableDiffuse = !toggleData.enableDiffuse;
            lightingToggleCB.UpdateBuffer(immediateContext, &toggleData);
            OutputDebugStringA(toggleData.enableDiffuse ? "Diffuse: ON\n" : "Diffuse: OFF\n");
        }
        key2Prev = key2Now;

        if (key3Now && !key3Prev)
        {
            toggleData.enableSpecular = !toggleData.enableSpecular;
            lightingToggleCB.UpdateBuffer(immediateContext, &toggleData);
            OutputDebugStringA(toggleData.enableSpecular ? "Specular: ON\n" : "Specular: OFF\n");
        }
        key3Prev = key3Now;

        if (key4Now && !key4Prev)
        {
            wireframeEnabled = !wireframeEnabled;
            OutputDebugStringA(wireframeEnabled ? "Wireframe: ON\n" : "Wireframe: OFF\n");
        }
        key4Prev = key4Now;

        if (key5Now && !key5Prev)
        {
            tessellationEnabled = !tessellationEnabled;
            OutputDebugStringA(tessellationEnabled ? "Tessellation: ON\n" : "Tessellation: OFF\n");
        }
        key5Prev = key5Now;

        // --- Camera movement + mouselook ---
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

        // Update rotating cube (GameObject 0)
        XMMATRIX translation = XMMatrixTranslation(x, 0.f, z);
        XMVECTOR objectPos = XMVectorSet(x, 0.f, z, 1.f);
        XMMATRIX lookAtMatrix = XMMatrixLookAtLH(objectPos, XMVectorZero(), XMVectorSet(0.f, 1.f, 0.f, 0.f));
        XMMATRIX worldMatrix = XMMatrixTranspose(lookAtMatrix) * translation;
        gameObjects[0].SetWorldMatrix(worldMatrix);

        ID3D11DepthStencilView* myDSV = depthBuffer.GetDSV(0);

  // ----- SHADOW PASS (Render depth from each light's perspective) -----
        {
      static bool firstShadowFrame = true;
            if (firstShadowFrame)
 {
  OutputDebugStringA("\n=== SHADOW PASS START (First Frame) ===\n");
            }
 
          // Set up for depth-only rendering
     immediateContext->IASetInputLayout(inputLayout.GetInputLayout());
            immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            immediateContext->VSSetShader(vShader, nullptr, 0);
            immediateContext->PSSetShader(nullptr, nullptr, 0); // No pixel shader for depth-only
       
   ID3D11Buffer* cb0 = constantBuffer.GetBuffer();
  immediateContext->VSSetConstantBuffers(0, 1, &cb0);
            
     // Render shadow map for each light
  for (size_t lightIndex = 0; lightIndex < lights.size(); ++lightIndex)
     {
     if (firstShadowFrame)
  {
          char debugBuf[256];
     sprintf_s(debugBuf, "  Light %zu: type=%d, enabled=%d, position=(%.2f,%.2f,%.2f), direction=(%.2f,%.2f,%.2f)\n", 
           lightIndex, lights[lightIndex].type, lights[lightIndex].enabled,
            lights[lightIndex].position.x, lights[lightIndex].position.y, lights[lightIndex].position.z,
         lights[lightIndex].direction.x, lights[lightIndex].direction.y, lights[lightIndex].direction.z);
        OutputDebugStringA(debugBuf);
        }
                
     // Bind shadow map DSV for this light
    ID3D11DepthStencilView* shadowDSV = shadowMap.GetDSV(static_cast<UINT>(lightIndex));
         if (!shadowDSV)
      {
        OutputDebugStringA("    ERROR: Shadow DSV is NULL!\n");
            continue;
          }
 
       // Clear depth
  immediateContext->ClearDepthStencilView(shadowDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
                
      // Set shadow map viewport
                D3D11_VIEWPORT shadowViewport = shadowMap.GetViewport();
  immediateContext->RSSetViewports(1, &shadowViewport);
       
        // Bind shadow DSV (no color target)
                ID3D11RenderTargetView* nullRTV = nullptr;
                immediateContext->OMSetRenderTargets(1, &nullRTV, shadowDSV);
        
     // Load light's view-projection matrix
        XMMATRIX lightVP = XMLoadFloat4x4(&lights[lightIndex].viewProj);
    
        // Render all game objects from light's perspective
            int objectCount = 0;
        for (auto& gameObject : gameObjects)
   {
      MatrixPair shadowData;
              XMMATRIX worldMat = gameObject.GetWorldMatrix();
  
         XMStoreFloat4x4(&shadowData.world, XMMatrixTranspose(worldMat));
   XMStoreFloat4x4(&shadowData.viewProj, XMMatrixTranspose(lightVP));
 
                constantBuffer.UpdateBuffer(immediateContext, &shadowData);
   
             // Draw depth only (no textures or material updates needed)
         const MeshD3D11* mesh = gameObject.GetMesh();
        if (mesh)
  {
            mesh->BindMeshBuffers(immediateContext);
      for (size_t i = 0; i < mesh->GetNrOfSubMeshes(); ++i)
     {
                  mesh->PerformSubMeshDrawCall(immediateContext, i);
          }
       objectCount++;
           }
    }
          
          if (firstShadowFrame)
      {
         char debugBuf[128];
       sprintf_s(debugBuf, "    Rendered %d objects to shadow map %zu\n", objectCount, lightIndex);
            OutputDebugStringA(debugBuf);
                }
          }
        
     if (firstShadowFrame)
  {
          OutputDebugStringA("=== SHADOW PASS END ===\n\n");
       firstShadowFrame = false;
      }
        }

        // ----- GEOMETRY PASS TO G-BUFFER -----
        {
            gbuffer.SetAsRenderTargets(immediateContext, myDSV);

            float gClear[4] = { 0.f, 0.f, 0.f, 0.f };
            gbuffer.Clear(immediateContext, gClear);

            immediateContext->ClearDepthStencilView(
                myDSV,
                D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                1.0f,
                0
            );

            immediateContext->RSSetViewports(1, &viewport);

            // Set rasterizer state based on wireframe toggle
            ID3D11RasterizerState* currentRasterizerState = wireframeEnabled ? wireframeRasterizerState : solidRasterizerState;
            immediateContext->RSSetState(currentRasterizerState);

            immediateContext->IASetInputLayout(inputLayout.GetInputLayout());
            
            ID3D11Buffer* cb0 = constantBuffer.GetBuffer();
            immediateContext->VSSetConstantBuffers(0, 1, &cb0);
            
            ID3D11SamplerState* samplerPtr = samplerState.GetSamplerState();
            immediateContext->PSSetSamplers(0, 1, &samplerPtr);

            // Choose pipeline based on tessellation toggle
            if (tessellationEnabled && tessVS && tessHS && tessDS)
            {
                // TESSELLATION PIPELINE
                immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
                immediateContext->VSSetShader(tessVS, nullptr, 0);
                immediateContext->HSSetShader(tessHS, nullptr, 0);
                immediateContext->DSSetShader(tessDS, nullptr, 0);
                immediateContext->DSSetConstantBuffers(0, 1, &cb0);
                immediateContext->PSSetShader(pShader, nullptr, 0);
            }
            else
            {
                // NORMAL PIPELINE (No Tessellation)
                immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                immediateContext->VSSetShader(vShader, nullptr, 0);
                immediateContext->HSSetShader(nullptr, nullptr, 0);
                immediateContext->DSSetShader(nullptr, nullptr, 0);
                immediateContext->PSSetShader(pShader, nullptr, 0);
            }

            // Render rotating quad with imageQuadVB
            {
                MatrixPair rotatingData;
                XMMATRIX worldMat = gameObjects[0].GetWorldMatrix();
                XMStoreFloat4x4(&rotatingData.world, XMMatrixTranspose(worldMat));
                XMStoreFloat4x4(&rotatingData.viewProj, XMMatrixTranspose(VIEW_PROJ));
                constantBuffer.UpdateBuffer(immediateContext, &rotatingData);

                UINT stride = sizeof(SimpleVertex);
                UINT offset = 0;
                ID3D11Buffer* quadVB = imageQuadVB.GetBuffer();
                immediateContext->IASetVertexBuffers(0, 1, &quadVB, &stride, &offset);
                immediateContext->PSSetShaderResources(0, 1, &pineappleTexView);
                immediateContext->Draw(6, 0);
                
                ID3D11ShaderResourceView* nullSRV = nullptr;
                immediateContext->PSSetShaderResources(0, 1, &nullSRV);
            }

            // Render all GameObjects
            for (auto& gameObject : gameObjects)
            {
                gameObject.Draw(immediateContext, constantBuffer, materialBuffer, VIEW_PROJ, whiteTexView);
                
                ID3D11ShaderResourceView* nullSRV = nullptr;
                immediateContext->PSSetShaderResources(0, 1, &nullSRV);
            }
        }

        // ----- LIGHTING PASS (COMPUTE SHADER) -----
        {
            if (!lightingCS)
            {
                OutputDebugStringA("ERROR: NO COMPUTE SHADER - Skipping lighting pass!\n");
                // Copy albedo directly to output as fallback
                if (lightingTex && gbuffer.GetAlbedoSRV())
                {
                    ID3D11Resource* albedoResource = nullptr;
                    gbuffer.GetAlbedoSRV()->GetResource(&albedoResource);
                    if (albedoResource)
                    {
                        immediateContext->CopyResource(lightingTex, (ID3D11Texture2D*)albedoResource);
                        albedoResource->Release();
                    }
                }
            }
            else
            {
                if (!gbuffer.GetAlbedoSRV())
                    OutputDebugStringA("GBuffer Albedo SRV is NULL!\n");
                if (!gbuffer.GetNormalSRV())
                    OutputDebugStringA("GBuffer Normal SRV is NULL!\n");
                if (!gbuffer.GetPositionSRV())
                    OutputDebugStringA("GBuffer Position SRV is NULL!\n");
                
                if (!lightStructuredBuffer.GetSRV())
                    OutputDebugStringA("ERROR: Light structured buffer SRV is NULL!\n");

                // Unbind RTs before using as SRV in compute
                {
                    ID3D11RenderTargetView* nullRTVs[3] = { nullptr, nullptr, nullptr };
                    immediateContext->OMSetRenderTargets(3, nullRTVs, nullptr);
                }

                // Bind G-buffer SRVs t0,t1,t2
                ID3D11ShaderResourceView* srvs[3] =
                {
                    gbuffer.GetAlbedoSRV(), // t0
                    gbuffer.GetNormalSRV(), // t1
                    gbuffer.GetPositionSRV()    // t2 (world pos)
                };
                immediateContext->CSSetShaderResources(0, 3, srvs);

                // Bind shadow map array at t3
                ID3D11ShaderResourceView* shadowSRV = shadowMap.GetSRV();
                if (shadowSRV)
                {
                    immediateContext->CSSetShaderResources(3, 1, &shadowSRV);
    
                    static bool firstLightingFrame = true;
  if (firstLightingFrame)
     {
    OutputDebugStringA("Shadow map SRV bound to t3 (VERIFIED)\n");
   firstLightingFrame = false;
 }
       }
         else
    {
     OutputDebugStringA("WARNING: Shadow map SRV is NULL!\n");
}

                // Bind light structured buffer t4 and camera to b2
                ID3D11ShaderResourceView* lightSRV = lightStructuredBuffer.GetSRV();
                immediateContext->CSSetShaderResources(4, 1, &lightSRV);
                immediateContext->CSSetConstantBuffers(2, 1, &cameraCB);

                // Bind toggle buffer to b4
                ID3D11Buffer* toggleCBBuf = lightingToggleCB.GetBuffer();
                immediateContext->CSSetConstantBuffers(4, 1, &toggleCBBuf);

                // Bind shadow sampler at s1
                if (shadowSampler)
   {
          immediateContext->CSSetSamplers(1, 1, &shadowSampler);
   OutputDebugStringA("Shadow sampler bound to s1\n");
   }

                // Bind UAV
                ID3D11UnorderedAccessView* uavs[1] = { lightingUAV };
                UINT initialCounts[1] = { 0 };
                immediateContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

                // Run compute
                immediateContext->CSSetShader(lightingCS, nullptr, 0);

                UINT groupsX = (WIDTH + 15) / 16;
                UINT groupsY = (HEIGHT + 15) / 16;
                immediateContext->Dispatch(groupsX, groupsY, 1);
                
                static bool firstFrame = true;
                if (firstFrame)
                {
                    char buf[128];
                    sprintf_s(buf, "Dispatching compute shader: %dx%d groups\n", groupsX, groupsY);
                    OutputDebugStringA(buf);
                    firstFrame = false;
                }

                // Cleanup bindings
                ID3D11ShaderResourceView* nullSRVs[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
                immediateContext->CSSetShaderResources(0, 5, nullSRVs);

                ID3D11SamplerState* nullSampler = nullptr;
    immediateContext->CSSetSamplers(1, 1, &nullSampler);

           ID3D11UnorderedAccessView* nullUAVs[1] = { nullptr };
                UINT zeros[1] = { 0 };
                immediateContext->CSSetUnorderedAccessViews(0, 1, nullUAVs, zeros);

                immediateContext->CSSetShader(nullptr, nullptr, 0);
            }
        }

        // Copy compute result to backbuffer
        ID3D11Texture2D* backBufferTex = nullptr;
        HRESULT hr = swapChain->GetBuffer(
            0,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(&backBufferTex)
        );

        if (SUCCEEDED(hr) && backBufferTex)
        {
            ID3D11Texture2D* src = lightingTex;
            if (src)
            {
                immediateContext->CopyResource(backBufferTex, src);
            }
            backBufferTex->Release();
        }

        swapChain->Present(0, 0);
    }

    // Cleanup
    if (tessVS) tessVS->Release();
    if (tessHS) tessHS->Release();
    if (tessDS) tessDS->Release();
    if (lightingUAV)  lightingUAV->Release();
    if (lightingTex)  lightingTex->Release();
    if (lightingCS)   lightingCS->Release();
    if (shadowSampler) shadowSampler->Release();
    if (solidRasterizerState) solidRasterizerState->Release();
    if (wireframeRasterizerState) wireframeRasterizerState->Release();

    if (pineappleTexView && pineappleTexView != whiteTexView) pineappleTexView->Release();

    whiteTexView->Release();
    textureView->Release();
    texture->Release();
    pShader->Release();
    vShader->Release();
    rtv->Release();
    swapChain->Release();
    immediateContext->Release();
    device->Release();
    UnloadMeshes();
    return 0;
}
