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
#include "QuadTree.h"

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

// Debug culling parameters
static float DEBUG_CULLING_FOV_MULTIPLIER = 0.6f;  // Makes the frustum 60% of the camera FOV

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

    // *** LÄGG TILL D3D11_BIND_RENDER_TARGET! ***
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    device->CreateTexture2D(&desc, nullptr, &texture);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = desc.Format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(texture, &uavDesc, &uav);
<<<<<<< Updated upstream
}

// Add cleanup function before wWinMain
void CleanupD3DResources(
	ID3D11Device*& device,
	ID3D11DeviceContext*& context,
	IDXGISwapChain*& swapChain,
	ID3D11RenderTargetView*& rtv,
	ID3D11RasterizerState*& solidRasterizerState,
	ID3D11RasterizerState*& wireframeRasterizerState,
	ID3D11VertexShader*& vShader,
	ID3D11PixelShader*& pShader,
	ID3D11VertexShader*& tessVS,
	ID3D11HullShader*& tessHS,
	ID3D11DomainShader*& tessDS,
	ID3D11PixelShader*& reflectionPS,
	ID3D11PixelShader*& cubeMapPS,
	ID3D11PixelShader*& normalMapPS,
	ID3D11PixelShader*& parallaxPS,
	ID3D11ComputeShader*& lightingCS,
	ID3D11SamplerState*& shadowSampler,
	ID3D11ShaderResourceView*& whiteTexView,
	ID3D11Texture2D*& lightingTex,
	ID3D11UnorderedAccessView*& lightingUAV
)
{
	if (lightingUAV) { lightingUAV->Release(); lightingUAV = nullptr; }
	if (lightingTex) { lightingTex->Release(); lightingTex = nullptr; }
	if (lightingCS) { lightingCS->Release(); lightingCS = nullptr; }
	if (parallaxPS) { parallaxPS->Release(); parallaxPS = nullptr; }
	if (normalMapPS) { normalMapPS->Release(); normalMapPS = nullptr; }
	if (cubeMapPS) { cubeMapPS->Release(); cubeMapPS = nullptr; }
	if (reflectionPS) { reflectionPS->Release(); reflectionPS = nullptr; }
	if (tessDS) { tessDS->Release(); tessDS = nullptr; }
	if (tessHS) { tessHS->Release(); tessHS = nullptr; }
	if (tessVS) { tessVS->Release(); tessVS = nullptr; }
	if (shadowSampler) { shadowSampler->Release(); shadowSampler = nullptr; }
	if (solidRasterizerState) { solidRasterizerState->Release(); solidRasterizerState = nullptr; }
	if (wireframeRasterizerState) { wireframeRasterizerState->Release(); wireframeRasterizerState = nullptr; }
	if (pShader) { pShader->Release(); pShader = nullptr; }
	if (vShader) { vShader->Release(); vShader = nullptr; }
	if (rtv) { rtv->Release(); rtv = nullptr; }
	if (swapChain) { swapChain->Release(); swapChain = nullptr; }
	if (context) { context->Release(); context = nullptr; }
	if (device) { device->Release(); device = nullptr; }
	UnloadMeshes();
=======
>>>>>>> Stashed changes
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
<<<<<<< Updated upstream
<<<<<<< Updated upstream
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

	// Normal map shader
	ID3D11PixelShader* normalMapPS = ShaderLoader::CreatePixelShader(device, "NormalMapPS.cso");

	// Parallax occlusion mapping shader
	ID3D11PixelShader* parallaxPS = ShaderLoader::CreatePixelShader(device, "ParallaxPS.cso");

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

	// Compute output
	ID3D11Texture2D* lightingTex = nullptr;
	ID3D11UnorderedAccessView* lightingUAV = nullptr;
	CreateComputeOutputResources(device, WIDTH, HEIGHT, lightingTex, lightingUAV);

	// Initialize these early so they're available for cleanup
	ID3D11ShaderResourceView* whiteTexView = nullptr;
	ID3D11SamplerState* shadowSampler = nullptr;

	// Shadow mapping
	ShadowMapD3D11 shadowMap;
	if (!shadowMap.Initialize(device, 2048, 2048, 4))
	{
		OutputDebugStringA("Failed to initialize Shadow Map!\n");
		CleanupD3DResources(device, context, swapChain, rtv, solidRasterizerState, wireframeRasterizerState,
			vShader, pShader, tessVS, tessHS, tessDS, reflectionPS, cubeMapPS, normalMapPS, parallaxPS,
			lightingCS, shadowSampler, whiteTexView, lightingTex, lightingUAV);
		return -1;
	}

	// Environment mapping
	EnvironmentMapRenderer envMapRenderer;
	if (!envMapRenderer.Initialize(device, 512))
	{
		OutputDebugStringA("Failed to initialize Environment Map!\n");
		CleanupD3DResources(device, context, swapChain, rtv, solidRasterizerState, wireframeRasterizerState,
			vShader, pShader, tessVS, tessHS, tessDS, reflectionPS, cubeMapPS, normalMapPS, parallaxPS,
			lightingCS, shadowSampler, whiteTexView, lightingTex, lightingUAV);
		return -1;
	}

	// Light manager
	LightManager lightManager;
	lightManager.InitializeDefaultLights(device);

	// Meshes
	const MeshD3D11* cubeMesh = GetMesh("cube.obj", device);
	const MeshD3D11* simpleCubeMesh = GetMesh("SimpleCube.obj", device);
	const MeshD3D11* sphereMesh = GetMesh("sphere.obj", device);
	const MeshD3D11* sphereNormalMapMesh = GetMesh("sphereNormalMap.obj", device);
	const MeshD3D11* simpleCubeNormal = GetMesh("SimpleCubeNormal.obj", device);
	const MeshD3D11* simpleCubeParallax = GetMesh("SimpleCubeParallax.obj", device);

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
	// Initialize materialBuffer with the material data
	ConstantBufferD3D11 materialBuffer(device, sizeof(Material), &mat);

	// Lighting toggles
	LightingToggles toggleData = { 0, 1, 1, 0 };
	// Initialize lightingToggleCB with the toggle data
	ConstantBufferD3D11 lightingToggleCB(device, sizeof(LightingToggles), &toggleData);

	// Camera
	ProjectionInfo proj{ FOV, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE };
	CameraD3D11 camera;
	camera.Initialize(device, proj, XMFLOAT3(0.0f, 5.0f, -10.0f));
	camera.RotateForward(XMConvertToRadians(-20.0f));

	// Textures - Now actually initialize them
	whiteTexView = TextureLoader::CreateWhiteTexture(device);

	// Samplers
	SamplerD3D11 samplerState;
	samplerState.Initialize(device, D3D11_TEXTURE_ADDRESS_WRAP);
	shadowSampler = CreateShadowSampler(device);

	// Game objects
	std::vector<GameObject> gameObjects;
	// Note: gameObjects[0] is now a spinning quad rendered separately (not a GameObject)
	gameObjects.emplace_back(cubeMesh);
	gameObjects[0].SetWorldMatrix(XMMatrixTranslation(30.0f, 1.0f, 0.0f));
	gameObjects.emplace_back(sphereMesh);  // Changed to sphereMesh
	gameObjects[1].SetWorldMatrix(XMMatrixTranslation(0.0f, 0.0f, -14.0f));
	gameObjects.emplace_back(sphereMesh);
	gameObjects[2].SetWorldMatrix(XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(4.0f, 2.0f, -2.0f));
	gameObjects.emplace_back(simpleCubeMesh);
	gameObjects[3].SetWorldMatrix(XMMatrixTranslation(2.0f, 2.0f, 0.0f));
	gameObjects.emplace_back(sphereMesh);
	gameObjects[4].SetWorldMatrix(XMMatrixScaling(5.0f, 0.2f, 5.0f) * XMMatrixTranslation(0.0f, -1.0f, 0.0f));
	gameObjects.emplace_back(sphereMesh);
	gameObjects[5].SetWorldMatrix(XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(2.0f, 3.0f, -3.0f));
	gameObjects.emplace_back(sphereMesh);
	gameObjects[6].SetWorldMatrix(XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(4.0f, 3.0f, -3.0f));
	gameObjects.emplace_back(sphereMesh);
	gameObjects[7].SetWorldMatrix(XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(6.0f, 3.0f, -3.0f));
	gameObjects.emplace_back(simpleCubeNormal);
	gameObjects[8].SetWorldMatrix(XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(5.0f, 2.0f, 0.0f));
	gameObjects.emplace_back(simpleCubeParallax);
	gameObjects[9].SetWorldMatrix(XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(-1.0f, 2.0f, 0.0f));

	// Add small spheres at each spotlight position
	const auto& lights = lightManager.GetLights();
	for (size_t i = 0; i < lights.size(); ++i)
	{
		// Skip directional light (type 0), only add spheres for spotlights (type 1)
		if (lights[i].type == 1)
		{
			gameObjects.emplace_back(sphereMesh);
			const XMFLOAT3& lightPos = lights[i].position;
			gameObjects.back().SetWorldMatrix(
				XMMatrixScaling(0.2f, 0.2f, 0.2f) *
				XMMatrixTranslation(lightPos.x, lightPos.y, lightPos.z)
			);
		}
	}


	const size_t REFLECTIVE_OBJECT_INDEX = 2;  // Reflective sphere at index 2
	const size_t NORMAL_MAP_OBJECT_INDEX = 8;  // SimpleCubeNormal at index 8
	const size_t PARALLAX_OBJECT_INDEX = 9;    // SimpleCubeParallax at index 9

	// Initialize QuadTree for frustum culling
	// Define world bounds covering the entire scene
	DirectX::BoundingBox worldBoundingBox(
		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),      // Center
		DirectX::XMFLOAT3(50.0f, 25.0f, 50.0f)    // Extents (half-width, half-height, half-depth)
	);
	QuadTree<GameObject*> sceneTree(worldBoundingBox, 5, 8);

	// Insert all game objects into the QuadTree
	for (auto& obj : gameObjects)
	{
		DirectX::BoundingBox worldBox = obj.GetWorldBoundingBox();
		sceneTree.Insert(&obj, worldBox);
	}

	// Output control instructions
	OutputDebugStringA("===========================================\n");
	OutputDebugStringA("CONTROLS:\n");
	OutputDebugStringA("===========================================\n");
	OutputDebugStringA("WASD/QE   - Camera movement\n");
	OutputDebugStringA("Mouse- Camera rotation\n");
	OutputDebugStringA("1         - Toggle albedo only\n");
	OutputDebugStringA("2         - Toggle diffuse lighting\n");
	OutputDebugStringA("3         - Toggle specular lighting\n");
	OutputDebugStringA("4         - Toggle wireframe mode\n");
	OutputDebugStringA("5         - Toggle tessellation\n");
	OutputDebugStringA("6         - Toggle DEBUG CULLING (smaller frustum)\n");
	OutputDebugStringA("ESC       - Exit\n");
	OutputDebugStringA("===========================================\n");


	// State
	bool tessellationEnabled = false;
	bool wireframeEnabled = false;
	bool debugCullingEnabled = false;
	auto previousTime = std::chrono::high_resolution_clock::now();
	float rotationAngle = 90.f;
	const float mouseSens = 0.1f;

	bool key1Prev = false, key2Prev = false, key3Prev = false, key4Prev = false, key5Prev = false, key6Prev = false;

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
		bool key6Now = (GetAsyncKeyState('6') & 0x8000) != 0;

		if (key1Now && !key1Prev) { toggleData.showAlbedoOnly = !toggleData.showAlbedoOnly; lightingToggleCB.UpdateBuffer(context, &toggleData); }
		if (key2Now && !key2Prev) { toggleData.enableDiffuse = !toggleData.enableDiffuse; lightingToggleCB.UpdateBuffer(context, &toggleData); }
		if (key3Now && !key3Prev) { toggleData.enableSpecular = !toggleData.enableSpecular; lightingToggleCB.UpdateBuffer(context, &toggleData); }
		if (key4Now && !key4Prev) { wireframeEnabled = !wireframeEnabled; }
		if (key5Now && !key5Prev) { tessellationEnabled = !tessellationEnabled; }
		if (key6Now && !key6Prev) { debugCullingEnabled = !debugCullingEnabled; }
		key1Prev = key1Now; key2Prev = key2Now; key3Prev = key3Now; key4Prev = key4Now; key5Prev = key5Now; key6Prev = key6Now;

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

		// Update SimpleCubeNormal rotation to see normal mapping effect
		gameObjects[NORMAL_MAP_OBJECT_INDEX].SetWorldMatrix(
			XMMatrixRotationY(rotationAngle) * XMMatrixTranslation(5.0f, 2.0f, 0.0f)
		);

		// Update SimpleCube next to it for comparison
		gameObjects[3].SetWorldMatrix(
			XMMatrixRotationY(rotationAngle) * XMMatrixTranslation(2.0f, 2.0f, 0.0f)
		);

		// Update SimpleCubeParallax rotation to see parallax mapping effect
		gameObjects[PARALLAX_OBJECT_INDEX].SetWorldMatrix(
			XMMatrixRotationY(rotationAngle) * XMMatrixTranslation(-1.0f, 2.0f, 0.0f)
		);

		// Update QuadTree for moving objects
		sceneTree.Clear();
		for (auto& obj : gameObjects)
		{
			DirectX::BoundingBox worldBox = obj.GetWorldBoundingBox();
			sceneTree.Insert(&obj, worldBox);
		}

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

				// Create light frustum for culling
				XMMATRIX lightProj = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.0f), 1.0f, 0.1f, 100.0f);
				DirectX::BoundingFrustum lightFrustum(lightProj);

				// Transform to world space (simplified - you may need to extract actual light view matrix)
				XMMATRIX invLightView = XMMatrixInverse(nullptr, lightVP * XMMatrixInverse(nullptr, lightProj));
				DirectX::BoundingFrustum worldLightFrustum;
				lightFrustum.Transform(worldLightFrustum, invLightView);

				// Query visible objects for this light
				std::vector<GameObject*> shadowCasters;
				sceneTree.Query(worldLightFrustum, shadowCasters);

				// Render shadow casters
				for (GameObject* objPtr : shadowCasters)
				{
					MatrixPair shadowData;
					XMStoreFloat4x4(&shadowData.world, XMMatrixTranspose(objPtr->GetWorldMatrix()));
					XMStoreFloat4x4(&shadowData.viewProj, XMMatrixTranspose(lightVP));
					constantBuffer.UpdateBuffer(context, &shadowData);

					const MeshD3D11* mesh = objPtr->GetMesh();
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

			// *** FRUSTUM CULLING WITH QUADTREE ***
			// Create the culling frustum (debug mode uses a smaller frustum)
			DirectX::BoundingFrustum cullingFrustum;
			if (debugCullingEnabled)
			{
				// Create a smaller frustum for debug visualization
				float debugFOV = FOV * DEBUG_CULLING_FOV_MULTIPLIER;
				XMMATRIX debugProj = XMMatrixPerspectiveFovLH(debugFOV, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE);
				cullingFrustum = DirectX::BoundingFrustum(debugProj);

				// Transform to world space
				XMFLOAT3 camPos = camera.GetPosition();
				XMFLOAT3 camForward = camera.GetForward();
				XMFLOAT3 camUp = camera.GetUp();
				XMVECTOR posV = XMLoadFloat3(&camPos);
				XMVECTOR lookAtV = XMVectorAdd(posV, XMLoadFloat3(&camForward));
				XMVECTOR upV = XMLoadFloat3(&camUp);
				XMMATRIX view = XMMatrixLookAtLH(posV, lookAtV, upV);
				XMMATRIX invView = XMMatrixInverse(nullptr, view);

				DirectX::BoundingFrustum worldFrustum;
				cullingFrustum.Transform(worldFrustum, invView);
				cullingFrustum = worldFrustum;
			}
			else
			{
				// Use the full camera frustum for culling
				cullingFrustum = camera.GetBoundingFrustum();
			}

			// Query visible objects from QuadTree
			std::vector<GameObject*> visibleObjects;
			sceneTree.Query(cullingFrustum, visibleObjects);

			// Render visible game objects
			for (GameObject* objPtr : visibleObjects)
			{
				// Find the index for special handling (reflective object)
				size_t objIdx = objPtr - &gameObjects[0];

				if (objIdx == REFLECTIVE_OBJECT_INDEX && reflectionPS)
				{
					context->PSSetShader(reflectionPS, nullptr, 0);
					ID3D11ShaderResourceView* envSRV = envMapRenderer.GetEnvironmentSRV();
					context->PSSetShaderResources(1, 1, &envSRV);
					context->PSSetSamplers(0, 1, &samplerPtr);

					objPtr->Draw(context, constantBuffer, materialBuffer, VIEW_PROJ, whiteTexView);

					ID3D11ShaderResourceView* nullSRV = nullptr;
					context->PSSetShaderResources(1, 1, &nullSRV);
					context->PSSetShader(pShader, nullptr, 0);
				}
				else if (objIdx == NORMAL_MAP_OBJECT_INDEX && normalMapPS)
				{
					// Use normal map shader for SimpleCubeNormal
					context->PSSetShader(normalMapPS, nullptr, 0);

					// Bind the normal map texture to slot t1 BEFORE drawing
					const MeshD3D11* mesh = objPtr->GetMesh();
					ID3D11ShaderResourceView* normalMapSRV = nullptr;
					if (mesh && mesh->GetNrOfSubMeshes() > 0)
					{
						normalMapSRV = mesh->GetNormalHeightSRV(0);
					}

					// Draw with custom normal map binding
					if (!mesh) continue;

					// Update Matrix Buffer
					MatrixPair matrixData;
					XMMATRIX world = objPtr->GetWorldMatrix();
					XMStoreFloat4x4(&matrixData.world, XMMatrixTranspose(world));
					XMStoreFloat4x4(&matrixData.viewProj, XMMatrixTranspose(VIEW_PROJ));
					constantBuffer.UpdateBuffer(context, &matrixData);

					// Bind Vertex/Index Buffers
					mesh->BindMeshBuffers(context);

					// Render Submeshes with normal map
					for (size_t i = 0; i < mesh->GetNrOfSubMeshes(); ++i)
					{
						// Update Material
						const auto& meshMat = mesh->GetMaterial(i);
						Material matData;
						matData.ambient = meshMat.ambient;
						matData.diffuse = meshMat.diffuse;
						matData.specular = meshMat.specular;
						matData.specularPower = meshMat.specularPower;
						materialBuffer.UpdateBuffer(context, &matData);

						// Bind diffuse texture (t0)
						ID3D11ShaderResourceView* texture = mesh->GetDiffuseSRV(i);
						if (!texture) texture = whiteTexView;
						context->PSSetShaderResources(0, 1, &texture);

						// Bind normal map texture (t1)
						if (normalMapSRV)
						{
							context->PSSetShaderResources(1, 1, &normalMapSRV);
						}

						// Draw Call
						mesh->PerformSubMeshDrawCall(context, i);
					}

					// Unbind normal map and restore default shader
					ID3D11ShaderResourceView* nullSRV = nullptr;
					context->PSSetShaderResources(1, 1, &nullSRV);
					context->PSSetShader(pShader, nullptr, 0);
				}
				else if (objIdx == PARALLAX_OBJECT_INDEX && parallaxPS)
				{
					// Use parallax occlusion mapping shader for SimpleCubeParallax
					context->PSSetShader(parallaxPS, nullptr, 0);

					// Get the mesh
					const MeshD3D11* mesh = objPtr->GetMesh();
					if (!mesh) continue;

					// Get normal/height map
					ID3D11ShaderResourceView* normalHeightSRV = nullptr;
					if (mesh->GetNrOfSubMeshes() > 0)
					{
						normalHeightSRV = mesh->GetNormalHeightSRV(0);
					}

					// Update Matrix Buffer
					MatrixPair matrixData;
					XMMATRIX world = objPtr->GetWorldMatrix();
					XMStoreFloat4x4(&matrixData.world, XMMatrixTranspose(world));
					XMStoreFloat4x4(&matrixData.viewProj, XMMatrixTranspose(VIEW_PROJ));
					constantBuffer.UpdateBuffer(context, &matrixData);

					// Bind Vertex/Index Buffers
					mesh->BindMeshBuffers(context);

					// Render Submeshes with parallax mapping
					for (size_t i = 0; i < mesh->GetNrOfSubMeshes(); ++i)
					{
						// Update Material
						const auto& meshMat = mesh->GetMaterial(i);
						Material matData;
						matData.ambient = meshMat.ambient;
						matData.diffuse = meshMat.diffuse;
						matData.specular = meshMat.specular;
						matData.specularPower = meshMat.specularPower;
						materialBuffer.UpdateBuffer(context, &matData);

						// Bind diffuse texture (t0)
						ID3D11ShaderResourceView* texture = mesh->GetDiffuseSRV(i);
						if (!texture) texture = whiteTexView;
						context->PSSetShaderResources(0, 1, &texture);

						// Bind normal/height map texture (t1)
						if (normalHeightSRV)
						{
							context->PSSetShaderResources(1, 1, &normalHeightSRV);
						}

						// Draw Call
						mesh->PerformSubMeshDrawCall(context, i);
					}

					// Unbind textures and restore default shader
					ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
					context->PSSetShaderResources(0, 2, nullSRVs);
					context->PSSetShader(pShader, nullptr, 0);
				}
				else
				{
					objPtr->Draw(context, constantBuffer, materialBuffer, VIEW_PROJ, whiteTexView);
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
	CleanupD3DResources(device, context, swapChain, rtv,
		solidRasterizerState, wireframeRasterizerState,
		vShader, pShader, tessVS, tessHS, tessDS,
		reflectionPS, cubeMapPS, normalMapPS, parallaxPS,
		lightingCS, shadowSampler,
		whiteTexView, lightingTex, lightingUAV);

	return 0;
=======
=======
>>>>>>> Stashed changes
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    const UINT WIDTH = 1024;
    const UINT HEIGHT = 576;

    // Window setup
    HWND window;
    SetupWindow(hInstance, WIDTH, HEIGHT, nCmdShow, window);

    POINT center = { (LONG)(WIDTH / 2), (LONG)(HEIGHT / 2) };
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
    ID3D11BlendState* particleBlendState = nullptr;
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blendDesc, &particleBlendState);

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

    // Compute output
    ID3D11Texture2D* lightingTex = nullptr;
    ID3D11UnorderedAccessView* lightingUAV = nullptr;
    CreateComputeOutputResources(device, WIDTH, HEIGHT, lightingTex, lightingUAV);

    //ParticleSystem
    ID3D11RenderTargetView* lightingRTV = nullptr;
    HRESULT hr = device->CreateRenderTargetView(lightingTex, nullptr, &lightingRTV);

    if (FAILED(hr) || !lightingRTV)
    {
        OutputDebugStringA("FATAL ERROR: Failed to create lightingRTV!\n");

        // Debug: Kolla om lightingTex är giltig
        if (!lightingTex)
        {
            OutputDebugStringA("ERROR: lightingTex is NULL!\n");
        }
        else
        {
            OutputDebugStringA("ERROR: lightingTex exists but RTV creation failed!\n");
        }
    }
    else
    {
        OutputDebugStringA("SUCCESS: lightingRTV created!\n");
    }

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
    // Initialize materialBuffer with the material data
    ConstantBufferD3D11 materialBuffer(device, sizeof(Material), &mat);

    // Lighting toggles
    LightingToggles toggleData = { 0, 1, 1, 0 };
    // Initialize lightingToggleCB with the toggle data
    ConstantBufferD3D11 lightingToggleCB(device, sizeof(LightingToggles), &toggleData);

    // Camera
    ProjectionInfo proj{ FOV, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE };
    CameraD3D11 camera;
    camera.Initialize(device, proj, XMFLOAT3(0.0f, 5.0f, -10.0f));
    camera.RotateForward(XMConvertToRadians(-20.0f));

    //Particlesystem
    ParticleSystemD3D11 particleSystem(device, 200, XMFLOAT3(-3.0f, 1.0f, 3.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    //200 particles, emitter at (0,5,5), yellow color

    // [NEW] Emitter toggle state (default ON)
    bool emitterEnabled = true;
    particleSystem.SetEmitterEnabled(true);

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
    // Note: gameObjects[0] is now a spinning quad rendered separately (not a GameObject)
    gameObjects.emplace_back(cubeMesh);
    gameObjects[0].SetWorldMatrix(XMMatrixTranslation(30.0f, 1.0f, 0.0f));
    gameObjects.emplace_back(pineAppleMesh);
    gameObjects[1].SetWorldMatrix(XMMatrixTranslation(0.0f, 0.0f, -14.0f));
    gameObjects.emplace_back(sphereMesh);  // Changed to sphereMesh
    gameObjects[2].SetWorldMatrix(XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(-2.0f, 2.0f, 0.0f));
    gameObjects.emplace_back(simpleCubeMesh);
    gameObjects[3].SetWorldMatrix(XMMatrixTranslation(2.0f, 2.0f, 0.0f));
    gameObjects.emplace_back(simpleCubeMesh);
    gameObjects[4].SetWorldMatrix(XMMatrixScaling(5.0f, 0.2f, 5.0f) * XMMatrixTranslation(0.0f, -1.0f, 0.0f));
    gameObjects.emplace_back(sphereMesh);
    gameObjects[5].SetWorldMatrix(XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(2.0f, 3.0f, -3.0f));
    gameObjects.emplace_back(sphereMesh);
    gameObjects[6].SetWorldMatrix(XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(4.0f, 3.0f, -3.0f));
    gameObjects.emplace_back(sphereMesh);
    gameObjects[7].SetWorldMatrix(XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(6.0f, 3.0f, -3.0f));

    // Add small spheres at each spotlight position
    const auto& lights = lightManager.GetLights();
    for (size_t i = 0; i < lights.size(); ++i)
    {
        // Skip directional light (type 0), only add spheres for spotlights (type 1)
        if (lights[i].type == 1)
        {
            gameObjects.emplace_back(sphereMesh);
            const XMFLOAT3& lightPos = lights[i].position;
            gameObjects.back().SetWorldMatrix(
                XMMatrixScaling(0.2f, 0.2f, 0.2f) *
                XMMatrixTranslation(lightPos.x, lightPos.y, lightPos.z)
            );
        }
    }

    const size_t REFLECTIVE_OBJECT_INDEX = 2;  // Reflective sphere at index 2

    // Initialize QuadTree for frustum culling
    // Define world bounds covering the entire scene
    DirectX::BoundingBox worldBoundingBox(
        DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),      // Center
        DirectX::XMFLOAT3(50.0f, 25.0f, 50.0f)    // Extents (half-width, half-height, half-depth)
    );
    QuadTree<GameObject*> sceneTree(worldBoundingBox, 5, 8);

    // Insert all game objects into the QuadTree
    for (auto& obj : gameObjects)
    {
        DirectX::BoundingBox worldBox = obj.GetWorldBoundingBox();
        sceneTree.Insert(&obj, worldBox);
    }

    // Output control instructions
    OutputDebugStringA("===========================================\n");
    OutputDebugStringA("CONTROLS:\n");
    OutputDebugStringA("===========================================\n");
    OutputDebugStringA("WASD/QE   - Camera movement\n");
    OutputDebugStringA("Mouse- Camera rotation\n");
    OutputDebugStringA("1         - Toggle albedo only\n");
    OutputDebugStringA("2         - Toggle diffuse lighting\n");
    OutputDebugStringA("3         - Toggle specular lighting\n");
    OutputDebugStringA("4         - Toggle wireframe mode\n");
    OutputDebugStringA("5         - Toggle tessellation\n");
    OutputDebugStringA("6         - Toggle DEBUG CULLING (smaller frustum)\n");
    OutputDebugStringA("9         - Toggle particle emitter\n"); // [NEW]
    OutputDebugStringA("ESC       - Exit\n");
    OutputDebugStringA("===========================================\n");

    // State
    bool tessellationEnabled = false;
    bool wireframeEnabled = false;
    bool debugCullingEnabled = false;
    auto previousTime = std::chrono::high_resolution_clock::now();
    float rotationAngle = 90.f;
    const float mouseSens = 0.1f;

    bool key1Prev = false, key2Prev = false, key3Prev = false, key4Prev = false, key5Prev = false, key6Prev = false;
    bool key9Prev = false; // [NEW]

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
        bool key6Now = (GetAsyncKeyState('6') & 0x8000) != 0;
        bool key9Now = (GetAsyncKeyState('9') & 0x8000) != 0; // [NEW]

        if (key1Now && !key1Prev) { toggleData.showAlbedoOnly = !toggleData.showAlbedoOnly; lightingToggleCB.UpdateBuffer(context, &toggleData); }
        if (key2Now && !key2Prev) { toggleData.enableDiffuse = !toggleData.enableDiffuse; lightingToggleCB.UpdateBuffer(context, &toggleData); }
        if (key3Now && !key3Prev) { toggleData.enableSpecular = !toggleData.enableSpecular; lightingToggleCB.UpdateBuffer(context, &toggleData); }
        if (key4Now && !key4Prev) { wireframeEnabled = !wireframeEnabled; }
        if (key5Now && !key5Prev) { tessellationEnabled = !tessellationEnabled; }
        if (key6Now && !key6Prev) { debugCullingEnabled = !debugCullingEnabled; }

        // [NEW] Toggle emitter on 9
        if (key9Now && !key9Prev)
        {
            emitterEnabled = !emitterEnabled;
            particleSystem.SetEmitterEnabled(emitterEnabled);
        }

        key1Prev = key1Now; key2Prev = key2Now; key3Prev = key3Now; key4Prev = key4Now; key5Prev = key5Now; key6Prev = key6Now;
        key9Prev = key9Now; // [NEW]

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

        // Update spinning quad transformation (no longer using gameObjects[0])
        XMMATRIX spinningQuadWorld = XMMatrixRotationY(rotationAngle) * XMMatrixTranslation(0.0f, 0.0f, 0.0f);

        // Update particle system
        particleSystem.Update(context, dt);

        // Update QuadTree for moving objects
        sceneTree.Clear();
        for (auto& obj : gameObjects)
        {
            DirectX::BoundingBox worldBox = obj.GetWorldBoundingBox();
            sceneTree.Insert(&obj, worldBox);
        }

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

                // Create light frustum for culling
                XMMATRIX lightProj = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.0f), 1.0f, 0.1f, 100.0f);
                DirectX::BoundingFrustum lightFrustum(lightProj);

                // Transform to world space (simplified - you may need to extract actual light view matrix)
                XMMATRIX invLightView = XMMatrixInverse(nullptr, lightVP * XMMatrixInverse(nullptr, lightProj));
                DirectX::BoundingFrustum worldLightFrustum;
                lightFrustum.Transform(worldLightFrustum, invLightView);

                // Query visible objects for this light
                std::vector<GameObject*> shadowCasters;
                sceneTree.Query(worldLightFrustum, shadowCasters);

                // Render shadow casters
                for (GameObject* objPtr : shadowCasters)
                {
                    MatrixPair shadowData;
                    XMStoreFloat4x4(&shadowData.world, XMMatrixTranspose(objPtr->GetWorldMatrix()));
                    XMStoreFloat4x4(&shadowData.viewProj, XMMatrixTranspose(lightVP));
                    constantBuffer.UpdateBuffer(context, &shadowData);

                    const MeshD3D11* mesh = objPtr->GetMesh();
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

            // Render spinning quad
            {
                MatrixPair rotatingData;
                XMStoreFloat4x4(&rotatingData.world, XMMatrixTranspose(spinningQuadWorld));
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

            // *** FRUSTUM CULLING WITH QUADTREE ***
            // Create the culling frustum (debug mode uses a smaller frustum)
            DirectX::BoundingFrustum cullingFrustum;
            if (debugCullingEnabled)
            {
                // Create a smaller frustum for debug visualization
                float debugFOV = FOV * DEBUG_CULLING_FOV_MULTIPLIER;
                XMMATRIX debugProj = XMMatrixPerspectiveFovLH(debugFOV, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE);
                cullingFrustum = DirectX::BoundingFrustum(debugProj);

                // Transform to world space
                XMFLOAT3 camPos = camera.GetPosition();
                XMFLOAT3 camForward = camera.GetForward();
                XMFLOAT3 camUp = camera.GetUp();
                XMVECTOR posV = XMLoadFloat3(&camPos);
                XMVECTOR lookAtV = XMVectorAdd(posV, XMLoadFloat3(&camForward));
                XMVECTOR upV = XMLoadFloat3(&camUp);
                XMMATRIX view = XMMatrixLookAtLH(posV, lookAtV, upV);
                XMMATRIX invView = XMMatrixInverse(nullptr, view);

                DirectX::BoundingFrustum worldFrustum;
                cullingFrustum.Transform(worldFrustum, invView);
                cullingFrustum = worldFrustum;
            }
            else
            {
                // Use the full camera frustum for culling
                cullingFrustum = camera.GetBoundingFrustum();
            }

            // Query visible objects from QuadTree
            std::vector<GameObject*> visibleObjects;
            sceneTree.Query(cullingFrustum, visibleObjects);

            // Render visible game objects
            for (GameObject* objPtr : visibleObjects)
            {
                // Find the index for special handling (reflective object)
                size_t objIdx = (size_t)(objPtr - &gameObjects[0]);

                if (objIdx == REFLECTIVE_OBJECT_INDEX && reflectionPS)
                {
                    context->PSSetShader(reflectionPS, nullptr, 0);
                    ID3D11ShaderResourceView* envSRV = envMapRenderer.GetEnvironmentSRV();
                    context->PSSetShaderResources(1, 1, &envSRV);
                    context->PSSetSamplers(0, 1, &samplerPtr);

                    objPtr->Draw(context, constantBuffer, materialBuffer, VIEW_PROJ, whiteTexView);

                    ID3D11ShaderResourceView* nullSRV = nullptr;
                    context->PSSetShaderResources(1, 1, &nullSRV);
                    context->PSSetShader(pShader, nullptr, 0);
                }
                else
                {
                    objPtr->Draw(context, constantBuffer, materialBuffer, VIEW_PROJ, whiteTexView);
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



        // ----- PARTICLE PASS -----
        {
            // *** DEBUG: Kolla lightingRTV ***
            if (!lightingRTV)
            {
                OutputDebugStringA("ERROR: lightingRTV is NULL in main!\n");
            }

            context->OMSetRenderTargets(1, &lightingRTV, myDSV);

            // *** DEBUG: Verifiera att binding funkade ***
            ID3D11RenderTargetView* checkRTV = nullptr;
            context->OMGetRenderTargets(1, &checkRTV, nullptr);
            if (!checkRTV)
            {
                OutputDebugStringA("ERROR: RTV not bound after OMSetRenderTargets!\n");
            }
            else
            {
                OutputDebugStringA("OK: RTV bound successfully in main\n");
                checkRTV->Release();
            }

            context->RSSetViewports(1, &viewport);
            context->RSSetState(solidRasterizerState);
            context->OMSetBlendState(particleBlendState, nullptr, 0xffffffff);

            particleSystem.Render(context, camera);

            context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
        }

        // Copy final result to backbuffer
        ID3D11Texture2D* backBuffer = nullptr;
        if (swapChain && SUCCEEDED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer)))
        {
            context->CopyResource(backBuffer, lightingTex);
            backBuffer->Release();
        }

        if (swapChain)
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
    if (lightingRTV) lightingRTV->Release();
    if (lightingCS) lightingCS->Release();
    if (shadowSampler) shadowSampler->Release();
    if (solidRasterizerState) solidRasterizerState->Release();
    if (particleBlendState) particleBlendState->Release();
    if (wireframeRasterizerState) wireframeRasterizerState->Release();
    if (pineappleTexView && pineappleTexView != whiteTexView) pineappleTexView->Release();
    if (whiteTexView) whiteTexView->Release();
    if (pShader) pShader->Release();
    if (vShader) vShader->Release();
    if (rtv) rtv->Release();

    if (swapChain) swapChain->Release();
    if (context) context->Release();
    if (device) device->Release();
    UnloadMeshes();

    return 0;
<<<<<<< Updated upstream
>>>>>>> Stashed changes
=======
>>>>>>> Stashed changes
}
