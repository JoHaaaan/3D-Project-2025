/* =============================================
 * ADVANCED REAL-TIME RENDERING TECHNIQUES DEMO
 * =============================================
 * 
 * RENDERING PIPELINE ARCHITECTURE:
 * 
 * 1. SHADOW MAPPING PASS
 *    - Depth-only rendering from each light's perspective
 *    - Stores depth information for shadow calculations
 * 
 * 2. ENVIRONMENT MAP PASS
 *    - Renders scene 6 times to create cube map for reflections
 *    - Used by reflective objects for dynamic environment mapping
 * 
 * 3. GEOMETRY PASS (Deferred Rendering)
 *    - Renders scene geometry to G-Buffer (3 render targets)
 *    - Stores: Albedo, Normals, World Position, Material properties
 *    - Special shaders for specific techniques:
 *      * Tessellation: Adaptive LOD based on camera distance
 *      * Reflection: Cube map sampling
 *      * Normal Mapping: Derivative-based TBN construction
 *      * Parallax Occlusion Mapping: Ray marching through height maps
 * 
 * 4. LIGHTING PASS (Compute Shader)
 *    - Reads G-Buffer and calculates lighting for all lights
 *    - Multi-light support with shadows
 *    - Outputs to UAV texture
 * 
 * 5. PARTICLE PASS (Forward Rendering)
 *    - GPU-based particle simulation using compute shaders
 *    - Geometry shader billboard expansion
 *    - Additive blending for effects
 * 
 * ADVANCED TECHNIQUES DEMONSTRATED:
 * - Deferred Rendering with G-Buffer
 * - Compute Shader Lighting & Particle Simulation
 * - Hardware Tessellation with Phong smoothing
 * - Shadow Mapping with PCF
 * - Dynamic Environment Mapping (Cube Maps)
 * - Normal Mapping (derivative-based)
 * - Parallax Occlusion Mapping
 * - QuadTree Spatial Partitioning for Frustum Culling
 * - Geometry Shader Amplification (Particles)
 * ============================================= */

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
#include "ParticleSystemD3D11.h"
using namespace DirectX;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Transformation parameters
static const float FOV = XMConvertToRadians(45.0f);
static const float ASPECT_RATIO = 1280.0f / 720.0f;
static const float NEAR_PLANE = 0.1f;
static const float FAR_PLANE = 100.0f;

// Global VIEW_PROJ matrix
XMMATRIX VIEW_PROJ;

// Debug culling parameters
static float DEBUG_CULLING_FOV_MULTIPLIER = 0.6f;

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

    // *** LaGG TILL D3D11_BIND_RENDER_TARGET! ***
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    device->CreateTexture2D(&desc, nullptr, &texture);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = desc.Format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(texture, &uavDesc, &uav);

}

// Add cleanup function before wWinMain
void CleanupD3DResources(
	ID3D11Device*& device,
	ID3D11DeviceContext*& context,
	IDXGISwapChain*& swapChain,
	ID3D11RenderTargetView*& rtv,
	ID3D11RasterizerState*& solidRasterizerState,
	ID3D11RasterizerState*& wireframeRasterizerState,
	ID3D11BlendState*& particleBlendState,
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
	ID3D11UnorderedAccessView*& lightingUAV,
	ID3D11RenderTargetView*& lightingRTV
)
{
	if (lightingRTV) { lightingRTV->Release(); lightingRTV = nullptr; }
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
	if (particleBlendState) { particleBlendState->Release(); particleBlendState = nullptr; }
	if (solidRasterizerState) { solidRasterizerState->Release(); solidRasterizerState = nullptr; }
	if (wireframeRasterizerState) { wireframeRasterizerState->Release(); wireframeRasterizerState = nullptr; }
	if (pShader) { pShader->Release(); pShader = nullptr; }
	if (vShader) { vShader->Release(); vShader = nullptr; }
	if (rtv) { rtv->Release(); rtv = nullptr; }
	if (whiteTexView) { whiteTexView->Release(); whiteTexView = nullptr; }
	if (swapChain) { swapChain->Release(); swapChain = nullptr; }
	if (context) { context->Release(); context = nullptr; }
	if (device) { device->Release(); device = nullptr; }
	UnloadMeshes();
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
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

	// [NEW] Particle blend state
	ID3D11BlendState* particleBlendState = nullptr;
	{
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
	}

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

	// [NEW] RTV for particle pass
	ID3D11RenderTargetView* lightingRTV = nullptr;
	if (lightingTex)
	{
		device->CreateRenderTargetView(lightingTex, nullptr, &lightingRTV);
	}

	// Initialize these early so they're available for cleanup
	ID3D11ShaderResourceView* whiteTexView = nullptr;
	ID3D11SamplerState* shadowSampler = nullptr;

	// Shadow mapping
	ShadowMapD3D11 shadowMap;
	if (!shadowMap.Initialize(device, 2048, 2048, 4))
	{
		OutputDebugStringA("Failed to initialize Shadow Map!\n");
		CleanupD3DResources(device, context, swapChain, rtv,
			solidRasterizerState, wireframeRasterizerState, particleBlendState,
			vShader, pShader, tessVS, tessHS, tessDS,
			reflectionPS, cubeMapPS, normalMapPS, parallaxPS,
			lightingCS, shadowSampler, whiteTexView, lightingTex, lightingUAV, lightingRTV);
		return -1;
	}

	// Environment mapping
	EnvironmentMapRenderer envMapRenderer;
	if (!envMapRenderer.Initialize(device, 512))
	{
		OutputDebugStringA("Failed to initialize Environment Map!\n");
		CleanupD3DResources(device, context, swapChain, rtv,
			solidRasterizerState, wireframeRasterizerState, particleBlendState,
			vShader, pShader, tessVS, tessHS, tessDS,
			reflectionPS, cubeMapPS, normalMapPS, parallaxPS,
			lightingCS, shadowSampler, whiteTexView, lightingTex, lightingUAV, lightingRTV);
		return -1;
	}

	// Light manager
	LightManager lightManager;
	lightManager.InitializeDefaultLights(device);

	// Meshes
	const MeshD3D11* cubeMesh = GetMesh("cube.obj", device);
	const MeshD3D11* simpleCubeMesh = GetMesh("SimpleCube.obj", device);
	const MeshD3D11* sphereMesh = GetMesh("sphere.obj", device);
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
	ConstantBufferD3D11 materialBuffer(device, sizeof(Material), &mat);

	// Lighting toggles
	LightingToggles toggleData = { 0, 1, 1, 0 };
	ConstantBufferD3D11 lightingToggleCB(device, sizeof(LightingToggles), &toggleData);

	// Camera
	ProjectionInfo proj{ FOV, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE };
	CameraD3D11 camera;
	camera.Initialize(device, proj, XMFLOAT3(0.0f, 5.0f, -10.0f));
	camera.RotateForward(XMConvertToRadians(-20.0f));

	// Textures
	whiteTexView = TextureLoader::CreateWhiteTexture(device);

	// Samplers
	SamplerD3D11 samplerState;
	samplerState.Initialize(device, D3D11_TEXTURE_ADDRESS_WRAP);
	shadowSampler = CreateShadowSampler(device);

	// [NEW] Particle system
	ParticleSystemD3D11 particleSystem(device, 200, XMFLOAT3(-3.0f, 1.0f, 3.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	ParticleSystemD3D11 particleSystem1(device, 200, XMFLOAT3(3.0f, 5.0f, 3.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f));

	bool emitterEnabled = true;
	particleSystem.SetEmitterEnabled(true);

	// Game objects
	std::vector<GameObject> gameObjects;
	gameObjects.emplace_back(cubeMesh);
	gameObjects[0].SetWorldMatrix(XMMatrixTranslation(30.0f, 1.0f, 0.0f));

	gameObjects.emplace_back(sphereMesh);
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
	gameObjects[8].SetWorldMatrix(XMMatrixTranslation(5.0f, 2.0f, 0.0f));

	gameObjects.emplace_back(simpleCubeParallax);
	gameObjects[9].SetWorldMatrix(XMMatrixTranslation(-1.0f, 2.0f, 0.0f));

	// Add small spheres at each spotlight position
	const auto& lights = lightManager.GetLights();
	for (size_t i = 0; i < lights.size(); ++i)
	{
		if (lights[i].type == 1)
		{
			gameObjects.emplace_back(sphereMesh);
			const XMFLOAT3& lightPos = lights[i].position;
			gameObjects.back().SetWorldMatrix(
				XMMatrixScaling(0.2f, 0.2f, 0.2f) *
				XMMatrixTranslation(lightPos.x, lightPos.y, lightPos.z));
		}
	}

	const size_t REFLECTIVE_OBJECT_INDEX = 2;
	const size_t NORMAL_MAP_OBJECT_INDEX = 8;
	const size_t PARALLAX_OBJECT_INDEX = 9;

	// QuadTree setup
	DirectX::BoundingBox worldBoundingBox(
		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT3(50.0f, 25.0f, 50.0f));
	QuadTree<GameObject*> sceneTree(worldBoundingBox, 5, 8);

	for (auto& obj : gameObjects)
	{
		sceneTree.Insert(&obj, obj.GetWorldBoundingBox());
	}

	// Controls output (add particle toggle)
	OutputDebugStringA("===========================================\n");
	OutputDebugStringA("CONTROLS:\n");
	OutputDebugStringA("===========================================\n");
	OutputDebugStringA("WASD/QE   - Camera movement\n");
	OutputDebugStringA("Mouse     - Camera rotation\n");
	OutputDebugStringA("1         - Toggle albedo only\n");
	OutputDebugStringA("2         - Toggle diffuse lighting\n");
	OutputDebugStringA("3         - Toggle specular lighting\n");
	OutputDebugStringA("4         - Toggle wireframe mode\n");
	OutputDebugStringA("5         - Toggle tessellation\n");
	OutputDebugStringA("6         - Toggle DEBUG CULLING (smaller frustum)\n");
	OutputDebugStringA("9         - Toggle particle emitter\n");
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
	bool key9Prev = false;

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

		// Input toggles
		bool key1Now = (GetAsyncKeyState('1') & 0x8000) != 0;
		bool key2Now = (GetAsyncKeyState('2') & 0x8000) != 0;
		bool key3Now = (GetAsyncKeyState('3') & 0x8000) != 0;
		bool key4Now = (GetAsyncKeyState('4') & 0x8000) != 0;
		bool key5Now = (GetAsyncKeyState('5') & 0x8000) != 0;
		bool key6Now = (GetAsyncKeyState('6') & 0x8000) != 0;
		bool key9Now = (GetAsyncKeyState('9') & 0x8000) != 0;

		if (key1Now && !key1Prev) { toggleData.showAlbedoOnly = !toggleData.showAlbedoOnly; }
		if (key2Now && !key2Prev) { toggleData.enableDiffuse = !toggleData.enableDiffuse; }
		if (key3Now && !key3Prev) { toggleData.enableSpecular = !toggleData.enableSpecular; }
		if (key1Now && !key1Prev || key2Now && !key2Prev || key3Now && !key3Prev) 
		{
			lightingToggleCB.UpdateBuffer(context, &toggleData);
		}
		if (key4Now && !key4Prev) { wireframeEnabled = !wireframeEnabled; }
		if (key5Now && !key5Prev) { tessellationEnabled = !tessellationEnabled; }
		if (key6Now && !key6Prev) { debugCullingEnabled = !debugCullingEnabled; }

		// [NEW] Toggle particle emitter on 9
		if (key9Now && !key9Prev)
		{
			emitterEnabled = !emitterEnabled;
			particleSystem.SetEmitterEnabled(emitterEnabled);
			particleSystem1.SetEmitterEnabled(emitterEnabled);
		}

		key1Prev = key1Now; key2Prev = key2Now; key3Prev = key3Now; key4Prev = key4Now;
		key5Prev = key5Now; key6Prev = key6Now; key9Prev = key9Now;

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

		// Update rotating demo objects
		gameObjects[NORMAL_MAP_OBJECT_INDEX].SetWorldMatrix(
			XMMatrixRotationY(rotationAngle) * XMMatrixTranslation(5.0f, 2.0f, 0.0f));
		gameObjects[3].SetWorldMatrix(
			XMMatrixRotationY(rotationAngle) * XMMatrixTranslation(2.0f, 2.0f, 0.0f));
		gameObjects[PARALLAX_OBJECT_INDEX].SetWorldMatrix(
			XMMatrixRotationY(rotationAngle) * XMMatrixTranslation(-1.0f, 2.0f, 0.0f));

		// [NEW] Update particles
		particleSystem.Update(context, dt);
		particleSystem1.Update(context, dt);

		// Update QuadTree
		sceneTree.Clear();
		for (auto& obj : gameObjects)
		{
			sceneTree.Insert(&obj, obj.GetWorldBoundingBox());
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

			auto& ls = lightManager.GetLights();
			for (size_t lightIdx = 0; lightIdx < ls.size(); ++lightIdx)
			{
				ID3D11DepthStencilView* shadowDSV = shadowMap.GetDSV(static_cast<UINT>(lightIdx));
				context->ClearDepthStencilView(shadowDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

				D3D11_VIEWPORT shadowViewport = shadowMap.GetViewport();
				context->RSSetViewports(1, &shadowViewport);

				ID3D11RenderTargetView* nullRTV = nullptr;
				context->OMSetRenderTargets(1, &nullRTV, shadowDSV);

				XMMATRIX lightVP = lightManager.GetLightViewProj(lightIdx);

				XMMATRIX lightProj = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.0f), 1.0f, 0.1f, 100.0f);
				DirectX::BoundingFrustum lightFrustum(lightProj);
				XMMATRIX invLightView = XMMatrixInverse(nullptr, lightVP * XMMatrixInverse(nullptr, lightProj));
				DirectX::BoundingFrustum worldLightFrustum;
				lightFrustum.Transform(worldLightFrustum, invLightView);

				std::vector<GameObject*> shadowCasters;
				sceneTree.Query(worldLightFrustum, shadowCasters);

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
				constantBuffer, materialBuffer, samplerState, whiteTexView);
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

			DirectX::BoundingFrustum cullingFrustum;
			if (debugCullingEnabled)
			{
				float debugFOV = FOV * DEBUG_CULLING_FOV_MULTIPLIER;
				XMMATRIX debugProj = XMMatrixPerspectiveFovLH(debugFOV, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE);
				cullingFrustum = DirectX::BoundingFrustum(debugProj);

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
				cullingFrustum = camera.GetBoundingFrustum();
			}

			std::vector<GameObject*> visibleObjects;
			sceneTree.Query(cullingFrustum, visibleObjects);

			for (GameObject* objPtr : visibleObjects)
			{
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
				else if (objIdx == NORMAL_MAP_OBJECT_INDEX && normalMapPS)
				{
					context->PSSetShader(normalMapPS, nullptr, 0);

					const MeshD3D11* mesh = objPtr->GetMesh();
					if (!mesh) continue;

					ID3D11ShaderResourceView* normalMapSRV = nullptr;
					if (mesh->GetNrOfSubMeshes() > 0)
						normalMapSRV = mesh->GetNormalHeightSRV(0);

					MatrixPair matrixData;
					XMMATRIX world = objPtr->GetWorldMatrix();
					XMStoreFloat4x4(&matrixData.world, XMMatrixTranspose(world));
					XMStoreFloat4x4(&matrixData.viewProj, XMMatrixTranspose(VIEW_PROJ));
					constantBuffer.UpdateBuffer(context, &matrixData);

					mesh->BindMeshBuffers(context);

					for (size_t i = 0; i < mesh->GetNrOfSubMeshes(); ++i)
					{
						const auto& meshMat = mesh->GetMaterial(i);
						Material matData;
						matData.ambient = meshMat.ambient;
						matData.diffuse = meshMat.diffuse;
						matData.specular = meshMat.specular;
						matData.specularPower = meshMat.specularPower;
						materialBuffer.UpdateBuffer(context, &matData);

						ID3D11ShaderResourceView* texture = mesh->GetDiffuseSRV(i);
						if (!texture) texture = whiteTexView;
						context->PSSetShaderResources(0, 1, &texture);

						if (normalMapSRV)
							context->PSSetShaderResources(1, 1, &normalMapSRV);

						mesh->PerformSubMeshDrawCall(context, i);
					}

					ID3D11ShaderResourceView* nullSRV = nullptr;
					context->PSSetShaderResources(1, 1, &nullSRV);
					context->PSSetShader(pShader, nullptr, 0);
				}
				else if (objIdx == PARALLAX_OBJECT_INDEX && parallaxPS)
				{
					context->PSSetShader(parallaxPS, nullptr, 0);

					const MeshD3D11* mesh = objPtr->GetMesh();
					if (!mesh) continue;

					ID3D11ShaderResourceView* normalHeightSRV = nullptr;
					if (mesh->GetNrOfSubMeshes() > 0)
						normalHeightSRV = mesh->GetNormalHeightSRV(0);

					MatrixPair matrixData;
					XMMATRIX world = objPtr->GetWorldMatrix();
					XMStoreFloat4x4(&matrixData.world, XMMatrixTranspose(world));
					XMStoreFloat4x4(&matrixData.viewProj, XMMatrixTranspose(VIEW_PROJ));
					constantBuffer.UpdateBuffer(context, &matrixData);

					mesh->BindMeshBuffers(context);

					for (size_t i = 0; i < mesh->GetNrOfSubMeshes(); ++i)
					{
						const auto& meshMat = mesh->GetMaterial(i);
						Material matData;
						matData.ambient = meshMat.ambient;
						matData.diffuse = meshMat.diffuse;
						matData.specular = meshMat.specular;
						matData.specularPower = meshMat.specularPower;
						materialBuffer.UpdateBuffer(context, &matData);

						ID3D11ShaderResourceView* texture = mesh->GetDiffuseSRV(i);
						if (!texture) texture = whiteTexView;
						context->PSSetShaderResources(0, 1, &texture);

						if (normalHeightSRV)
							context->PSSetShaderResources(1, 1, &normalHeightSRV);

						mesh->PerformSubMeshDrawCall(context, i);
					}

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

			ID3D11ShaderResourceView* nullSRVs[5] = { nullptr };
			context->CSSetShaderResources(0, 5, nullSRVs);
			ID3D11UnorderedAccessView* nullUAV = nullptr;
			context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
			context->CSSetShader(nullptr, nullptr, 0);
		}

		// ----- PARTICLE PASS -----
		if (lightingRTV && particleBlendState)
		{
			context->OMSetRenderTargets(1, &lightingRTV, myDSV);
			context->RSSetViewports(1, &viewport);
			context->RSSetState(solidRasterizerState);
			context->OMSetBlendState(particleBlendState, nullptr, 0xffffffff);

			particleSystem.Render(context, camera);
			particleSystem1.Render(context, camera);

			context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
		}

		// Copy to backbuffer
		ID3D11Texture2D* backBuffer = nullptr;
		if (swapChain && SUCCEEDED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer)))
		{
			context->CopyResource(backBuffer, lightingTex);
			backBuffer->Release();
		}

		swapChain->Present(0, 0);
	}

	// Cleanup
	CleanupD3DResources(device, context, swapChain, rtv,
		solidRasterizerState, wireframeRasterizerState, particleBlendState,
		vShader, pShader, tessVS, tessHS, tessDS,
		reflectionPS, cubeMapPS, normalMapPS, parallaxPS,
		lightingCS, shadowSampler, whiteTexView, lightingTex, lightingUAV, lightingRTV);

	return 0;
}

