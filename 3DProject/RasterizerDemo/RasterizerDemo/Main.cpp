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
struct Light
{
    XMFLOAT3 position;
    float    intensity;
    XMFLOAT3 color;
    float    padding;
};

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
    int enableDiffuse;  // 1 = enable diffuse
    int enableSpecular; // 1 = enable specular
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
    ID3D11RenderTargetView* rtv = nullptr; // not really used now
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
    ConstantBufferD3D11 lightingToggleCB; // NEW constant buffer for toggles
    CameraD3D11 camera;

    // Compute related
    ID3D11ComputeShader* lightingCS = nullptr;
    ID3D11Texture2D* lightingTex = nullptr;
    ID3D11UnorderedAccessView* lightingUAV = nullptr;

    SetupD3D11(WIDTH, HEIGHT, window, device, immediateContext, swapChain, rtv, viewport);

    std::string vShaderByteCode;
    SetupPipeline(device, vertexBuffer, vShader, pShader, vShaderByteCode);

    // Depth buffer
    DepthBufferD3D11 depthBuffer(device, WIDTH, HEIGHT, false);

    // G-buffer
    GBufferD3D11 gbuffer;
    gbuffer.Initialize(device, WIDTH, HEIGHT);

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
    
    // Light buffer
    Light lightData{ XMFLOAT3(0.0f, 0.0f, 4.0f), 1.0f, XMFLOAT3(1.0f, 1.0f, 1.0f), 0.0f };
    lightBuffer.Initialize(device, sizeof(Light), &lightData);

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
    camera.Initialize(device, proj, XMFLOAT3(0.0f, 0.0f, 4.0f));
    
    // Rotate camera 180 degrees (PI radians) around Y-axis at spawn
    camera.RotateRight(XM_PI);

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

    ID3D11Buffer* lightCB = lightBuffer.GetBuffer();
    ID3D11Buffer* materialCB = materialBuffer.GetBuffer();
    ID3D11Buffer* cameraCB = camera.GetConstantBuffer();

    // Still bound to PS for geometry pass
    immediateContext->PSSetConstantBuffers(1, 1, &lightCB);
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

    // Static cube world matrix
    XMMATRIX staticCubeWorld = XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(30.0f, 1.0f, 0.0f);

    // Pineapple world matrix (at position 10, 0, 0)
    XMMATRIX pineappleWorld = XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, 0.0f, -14.0f);

    // SimpleCube world matrix (at origin)
    XMMATRIX simpleCubeWorld = XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(-2.0f, 2.0f, 0.0f);

    // Toggle key state
    static bool key1Prev = false;
    static bool key2Prev = false;
    static bool key3Prev = false;

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

        // --- Toggle handling (1,2,3) ---
        short k1 = GetAsyncKeyState('1');
        short k2 = GetAsyncKeyState('2');
        short k3 = GetAsyncKeyState('3');

        bool key1Now = (k1 & 0x8000) != 0;
        bool key2Now = (k2 & 0x8000) != 0;
        bool key3Now = (k3 & 0x8000) != 0;

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

        // Debug camera position
        /*
        {
            XMFLOAT3 p = camera.GetPosition();
            char buf[128];
            sprintf_s(buf, "Camera Pos: X=%.3f  Y=%.3f  Z=%.3f\n", p.x, p.y, p.z);
            OutputDebugStringA(buf);
        }
        */
        ID3D11DepthStencilView* myDSV = depthBuffer.GetDSV(0);

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

            immediateContext->IASetInputLayout(inputLayout.GetInputLayout());
            immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            immediateContext->VSSetShader(vShader, nullptr, 0);
            immediateContext->PSSetShader(pShader, nullptr, 0);

            ID3D11Buffer* cb0 = constantBuffer.GetBuffer();
          immediateContext->VSSetConstantBuffers(0, 1, &cb0);
            // Don't bind a global texture here - let each object bind its own texture
            ID3D11SamplerState* samplerPtr = samplerState.GetSamplerState();
            immediateContext->PSSetSamplers(0, 1, &samplerPtr);

            // Rotating cube
            {
                MatrixPair rotatingData;
                XMStoreFloat4x4(&rotatingData.world, XMMatrixTranspose(worldMatrix));
                XMStoreFloat4x4(&rotatingData.viewProj, XMMatrixTranspose(VIEW_PROJ));
                constantBuffer.UpdateBuffer(immediateContext, &rotatingData);

                // Rotating image plane (quad with pineapple2.jpeg texture for testing)
                UINT stride = sizeof(SimpleVertex);
                UINT offset = 0;
                ID3D11Buffer* quadVB = imageQuadVB.GetBuffer();
                immediateContext->IASetVertexBuffers(0, 1, &quadVB, &stride, &offset);
  
                // Bind the pineapple texture for testing
                immediateContext->PSSetShaderResources(0, 1, &pineappleTexView);
    
                // Draw the quad (6 vertices = 2 triangles)
                immediateContext->Draw(6, 0);
            
                // Unbind texture§
                ID3D11ShaderResourceView* nullSRV = nullptr;
                immediateContext->PSSetShaderResources(0, 1, &nullSRV);
   }

            // Static cube
            {
     // Debug flag for first frame output
          static bool cubeDebugFirstFrame = true;
     
MatrixPair staticData;
      XMStoreFloat4x4(&staticData.world, XMMatrixTranspose(staticCubeWorld));
         XMStoreFloat4x4(&staticData.viewProj, XMMatrixTranspose(VIEW_PROJ));
       constantBuffer.UpdateBuffer(immediateContext, &staticData);

     if (cubeMesh)
     {
           cubeMesh->BindMeshBuffers(immediateContext);
  for (size_t i = 0; i < cubeMesh->GetNrOfSubMeshes(); ++i)
        {
   updateMaterialBufferForSubMesh(cubeMesh, i);

  // Debug: print the material being used
        if (cubeDebugFirstFrame)
              {
      const auto& matData = cubeMesh->GetMaterial(i);
              char matBuf[512];
     sprintf_s(matBuf, "  Material %zu: Kd=(%.3f, %.3f, %.3f)\n",
      i, matData.diffuse.x, matData.diffuse.y, matData.diffuse.z);
           OutputDebugStringA(matBuf);
         }

       // Bind diffuse texture for this submesh (t0).
       ID3D11ShaderResourceView* subDiffuse = cubeMesh->GetDiffuseSRV(i);
       ID3D11ShaderResourceView* toBind = subDiffuse ? subDiffuse : whiteTexView;

      // Debug output to see what texture is being used
   if (cubeDebugFirstFrame)
       {
         char buf[256];
sprintf_s(buf, "Cube submesh %zu: diffuseSRV = %p, using %s texture\n", 
    i, 
subDiffuse, 
    subDiffuse ? "LOADED" : "WHITE FALLBACK");
       OutputDebugStringA(buf);
    }
        
            immediateContext->PSSetShaderResources(0, 1, &toBind);

     cubeMesh->PerformSubMeshDrawCall(immediateContext, i);

             // Optional: unbind after draw to avoid keeping resource bound
  ID3D11ShaderResourceView* nullSRV = nullptr;
        immediateContext->PSSetShaderResources(0, 1, &nullSRV);
        }
        cubeDebugFirstFrame = false;
       }
     }

            // Pineapple (static, no animation for now)
            {
                // Debug flag for first frame output
                static bool pineappleDebugFirstFrame = true;

                MatrixPair staticPineappleData;
                XMStoreFloat4x4(&staticPineappleData.world, XMMatrixTranspose(pineappleWorld));
                XMStoreFloat4x4(&staticPineappleData.viewProj, XMMatrixTranspose(VIEW_PROJ));
                constantBuffer.UpdateBuffer(immediateContext, &staticPineappleData);

                if (pineAppleMesh)
                {
                    pineAppleMesh->BindMeshBuffers(immediateContext);
                    for (size_t i = 0; i < pineAppleMesh->GetNrOfSubMeshes(); ++i)
                    {
                        // Update material buffer for pineapple submesh
                        updateMaterialBufferForSubMesh(pineAppleMesh, i);

                        // Debug: print the material being used
                        if (pineappleDebugFirstFrame)
                        {
                            const auto& matData = pineAppleMesh->GetMaterial(i);
                            char matBuf[512];
                            sprintf_s(matBuf, "Pineapple Material %zu: Kd=(%.3f, %.3f, %.3f), Ks=(%.3f, %.3f, %.3f), Ns=%.3f\n",
                                i, matData.diffuse.x, matData.diffuse.y, matData.diffuse.z,
                                matData.specular.x, matData.specular.y, matData.specular.z,
                                matData.specularPower);
                            OutputDebugStringA(matBuf);
                        }

                        // Bind texture from MTL file (just like the cube does)
                        ID3D11ShaderResourceView* subDiffuse = pineAppleMesh->GetDiffuseSRV(i);
                        ID3D11ShaderResourceView* toBind = subDiffuse ? subDiffuse : whiteTexView;

                        // Debug output to see what texture is being used
                        if (pineappleDebugFirstFrame)
                        {
                            char buf[256];
                            sprintf_s(buf, "Pineapple submesh %zu: diffuseSRV = %p, using %s texture\n",
                                i,
                                subDiffuse,
                                subDiffuse ? "LOADED" : "WHITE FALLBACK");
                            OutputDebugStringA(buf);
                        }

                        immediateContext->PSSetShaderResources(0, 1, &toBind);

                        pineAppleMesh->PerformSubMeshDrawCall(immediateContext, i);

                        // Unbind after draw
                        ID3D11ShaderResourceView* nullSRV = nullptr;
                        immediateContext->PSSetShaderResources(0, 1, &nullSRV);
                    }
                    pineappleDebugFirstFrame = false;
                }
            }

            // SimpleCube (at origin)
            {
                // Debug flag for first frame output
                static bool simpleCubeDebugFirstFrame = true;

                MatrixPair simpleCubeData;
                XMStoreFloat4x4(&simpleCubeData.world, XMMatrixTranspose(simpleCubeWorld));
                XMStoreFloat4x4(&simpleCubeData.viewProj, XMMatrixTranspose(VIEW_PROJ));
                constantBuffer.UpdateBuffer(immediateContext, &simpleCubeData);

                if (simpleCubeMesh)
                {
                    simpleCubeMesh->BindMeshBuffers(immediateContext);
                    for (size_t i = 0; i < simpleCubeMesh->GetNrOfSubMeshes(); ++i)
                    {
                        // Debug: print the material being used
                        if (simpleCubeDebugFirstFrame)
                        {
                            const auto& matData = simpleCubeMesh->GetMaterial(i);
                            char matBuf[512];
                            sprintf_s(matBuf, "SimpleCube Material %zu: Kd=(%.3f, %.3f, %.3f), Ks=(%.3f, %.3f, %.3f), Ns=%.3f\n",
                                i, matData.diffuse.x, matData.diffuse.y, matData.diffuse.z,
                                matData.specular.x, matData.specular.y, matData.specular.z,
                                matData.specularPower);
                            OutputDebugStringA(matBuf);
                        }

                        // TEMPORARY: Force a good material for testing
                        Material testMaterial{
                            XMFLOAT3(0.2f, 0.2f, 0.2f), 0.f,  // ambient
                            XMFLOAT3(0.8f, 0.8f, 0.8f), 0.f,  // diffuse
                            XMFLOAT3(1.0f, 1.0f, 1.0f), 100.f // specular with high power
                        };
                        materialBuffer.UpdateBuffer(immediateContext, &testMaterial);

                        // Bind texture from MTL file
                        ID3D11ShaderResourceView* subDiffuse = simpleCubeMesh->GetDiffuseSRV(i);
                        ID3D11ShaderResourceView* toBind = subDiffuse ? subDiffuse : whiteTexView;

                        // Debug output to see what texture is being used
                        if (simpleCubeDebugFirstFrame)
                        {
                            char buf[256];
                            sprintf_s(buf, "SimpleCube submesh %zu: diffuseSRV = %p, using %s texture\n",
                                i,
                                subDiffuse,
                                subDiffuse ? "LOADED" : "WHITE FALLBACK");
                            OutputDebugStringA(buf);
                        }

                        immediateContext->PSSetShaderResources(0, 1, &toBind);

                        simpleCubeMesh->PerformSubMeshDrawCall(immediateContext, i);

                        // Unbind after draw
                        ID3D11ShaderResourceView* nullSRV = nullptr;
                        immediateContext->PSSetShaderResources(0, 1, &nullSRV);
                    }
                    simpleCubeDebugFirstFrame = false;
                }
            }
        }

        // ----- LIGHTING PASS (COMPUTE SHADER) -----
        {
            if (!gbuffer.GetAlbedoSRV())
                OutputDebugStringA("GBuffer Albedo SRV is NULL!\n");
            if (!gbuffer.GetNormalSRV())
                OutputDebugStringA("GBuffer Normal SRV is NULL!\n");
            if (!gbuffer.GetSpecSRV())
                OutputDebugStringA("GBuffer Spec SRV is NULL!\n");

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
                gbuffer.GetSpecSRV()    // t2 (world pos)
            };
            immediateContext->CSSetShaderResources(0, 3, srvs);

            // Bind light + camera to b1,b2
            ID3D11Buffer* csCBs[2] = { lightCB, cameraCB };
            immediateContext->CSSetConstantBuffers(1, 2, csCBs);

            // Bind toggle buffer to b4
            ID3D11Buffer* toggleCBBuf = lightingToggleCB.GetBuffer();
            immediateContext->CSSetConstantBuffers(4, 1, &toggleCBBuf);

            // Bind UAV
            ID3D11UnorderedAccessView* uavs[1] = { lightingUAV };
            UINT initialCounts[1] = { 0 };
            immediateContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

            // Run compute
            immediateContext->CSSetShader(lightingCS, nullptr, 0);

            UINT groupsX = (WIDTH + 15) / 16;
            UINT groupsY = (HEIGHT + 15) / 16;
            immediateContext->Dispatch(groupsX, groupsY, 1);

            // Cleanup bindings
            ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
            immediateContext->CSSetShaderResources(0, 3, nullSRVs);

            ID3D11UnorderedAccessView* nullUAVs[1] = { nullptr };
            UINT zeros[1] = { 0 };
            immediateContext->CSSetUnorderedAccessViews(0, 1, nullUAVs, zeros);

            immediateContext->CSSetShader(nullptr, nullptr, 0);
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
    if (lightingUAV)  lightingUAV->Release();
    if (lightingTex)  lightingTex->Release();
    if (lightingCS)   lightingCS->Release();

    if (pineappleTexView && pineappleTexView != whiteTexView) pineappleTexView->Release();
    if (whiteTexView) whiteTexView->Release();
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
