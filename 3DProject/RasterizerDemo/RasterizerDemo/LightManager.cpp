#include "LightManager.h"
#include <Windows.h>
#include <cmath>

using namespace DirectX;

// LIGHT MANAGER - Multi-Light System
// Manages directional and spot lights with shadow mapping support
// Key techniques: Structured buffer for light data, light-space transformations

void LightManager::InitializeDefaultLights(ID3D11Device* device)
{
	m_lights.resize(4);

	// Main directional light (sun)
	SetupDirectionalLight(m_lights[0]);

	// Three spotlights positioned around the scene
	SetupSpotLight(m_lights[1],
		XMFLOAT3(1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.0f, 10.0f, 0.0f),
		XMFLOAT3(0.0f, -1.0f, 0.0f));

	SetupSpotLight(m_lights[2],
		XMFLOAT3(1.0f, 1.0f, 1.0f),
		XMFLOAT3(-10.0f, 5.0f, -5.0f),
		XMFLOAT3(1.0f, -0.5f, 1.0f));

	SetupSpotLight(m_lights[3],
		XMFLOAT3(1.0f, 1.0f, 1.0f),
		XMFLOAT3(10.0f, 5.0f, -5.0f),
		XMFLOAT3(-1.0f, -0.5f, 1.0f));

	// Create structured buffer for all lights (accessible in compute shader)
	m_lightBuffer.Initialize(device, sizeof(LightData), static_cast<UINT>(m_lights.size()), m_lights.data());
}

void LightManager::SetupDirectionalLight(LightData& light)
{
	light.type = 0;
	light.enabled = 1;
	light.color = XMFLOAT3(1.0f, 1.0f, 1.0f);
	light.intensity = 0.8f;
	light.position = XMFLOAT3(20.0f, 30.0f, -20.0f);
	light.direction = XMFLOAT3(-1.0f, -1.0f, 1.0f);

	// Build light-space matrix for shadow mapping (orthographic for directional)
	XMVECTOR lightPos = XMLoadFloat3(&light.position);
	XMVECTOR lightDir = XMLoadFloat3(&light.direction);
	XMMATRIX view = XMMatrixLookToLH(lightPos, lightDir, XMVectorSet(0, 1, 0, 0));
	XMMATRIX proj = XMMatrixOrthographicLH(60.0f, 60.0f, 1.0f, 100.0f);

	XMStoreFloat4x4(&light.viewProj, view * proj);
}

void LightManager::SetupSpotLight(LightData& light, const XMFLOAT3& color,
	const XMFLOAT3& position, const XMFLOAT3& direction)
{
	light.type = 1;
	light.enabled = 1;
	light.color = color;
	light.intensity = 1.0f;
	light.position = position;
	light.direction = direction;
	light.range = 50.0f;
	light.spotAngle = XMConvertToRadians(60.0f);

	XMVECTOR lightPos = XMLoadFloat3(&light.position);
	XMVECTOR lightDir = XMLoadFloat3(&light.direction);

	// Handle up vector for look-at matrix (avoid gimbal lock)
	XMVECTOR upVec = XMVectorSet(0, 1, 0, 0);
	if (fabsf(direction.y) > 0.9f)
	{
		upVec = XMVectorSet(1, 0, 0, 0);
	}

	// Build light-space matrix for shadow mapping (perspective for spotlight)
	XMMATRIX view = XMMatrixLookToLH(lightPos, lightDir, upVec);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(light.spotAngle, 1.0f, 0.5f, 50.0f);

	XMStoreFloat4x4(&light.viewProj, view * proj);
}

DirectX::XMMATRIX LightManager::GetLightViewProj(size_t index) const
{
	if (index >= m_lights.size())
		return XMMatrixIdentity();

	return XMLoadFloat4x4(&m_lights[index].viewProj);
}