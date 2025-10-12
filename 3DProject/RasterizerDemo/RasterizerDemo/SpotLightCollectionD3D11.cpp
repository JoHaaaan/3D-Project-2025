#include "SpotLightCollectionD3D11.h"

using namespace DirectX;

void SpotLightCollectionD3D11::Initialize(ID3D11Device* device, const SpotLightData& lightInfo)
{
	// Initialize shadow maps
	shadowMaps.Initialize(device, 
		lightInfo.shadowMapInfo.textureDimension,
		lightInfo.shadowMapInfo.textureDimension,
		true, 
		static_cast<UINT>(lightInfo.perLightInfo.size()));

	// Initialize buffer data and cameras
	bufferData.resize(lightInfo.perLightInfo.size());
	shadowCameras.reserve(lightInfo.perLightInfo.size());

	for (size_t i = 0; i < lightInfo.perLightInfo.size(); ++i)
	{
		const auto& lightData = lightInfo.perLightInfo[i];

		// Setup camera for this light
		ProjectionInfo projInfo;
		projInfo.fovAngleY = lightData.angle * 2.0f; // Convert half-angle to full FOV
		projInfo.aspectRatio = 1.0f; // Square shadow maps
		projInfo.nearZ = lightData.projectionNearZ;
		projInfo.farZ = lightData.projectionFarZ;

		CameraD3D11 camera;
		camera.Initialize(device, projInfo, lightData.initialPosition);

		// Apply rotations to set light direction
		camera.RotateForward(lightData.rotationX);
		camera.RotateRight(lightData.rotationY);

		shadowCameras.push_back(std::move(camera));

		// Initialize buffer data
		bufferData[i].colour = lightData.colour;
		bufferData[i].angle = lightData.angle;
		bufferData[i].position = lightData.initialPosition;
		
		// Calculate direction from camera forward vector
		XMFLOAT3 forward = shadowCameras[i].GetForward();
		bufferData[i].direction = forward;

		// Get view-projection matrix
		XMFLOAT4X4 vp = shadowCameras[i].GetViewProjectionMatrix();
		bufferData[i].vpMatrix = vp;
	}

	// Initialize structured buffer for lights
	lightBuffer.Initialize(device, sizeof(LightBuffer),
		bufferData.size(), bufferData.data(), true);
}

void SpotLightCollectionD3D11::UpdateLightBuffers(ID3D11DeviceContext* context)
{
	// Update buffer data from cameras
	for (size_t i = 0; i < shadowCameras.size(); ++i)
	{
		XMFLOAT4X4 vp = shadowCameras[i].GetViewProjectionMatrix();
		bufferData[i].vpMatrix = vp;
		bufferData[i].position = shadowCameras[i].GetPosition();
		bufferData[i].direction = shadowCameras[i].GetForward();
	}

	// Update GPU buffer
	lightBuffer.UpdateBuffer(context, bufferData.data());
}

UINT SpotLightCollectionD3D11::GetNrOfLights() const
{
	return static_cast<UINT>(shadowCameras.size());
}

ID3D11DepthStencilView* SpotLightCollectionD3D11::GetShadowMapDSV(UINT lightIndex) const
{
	return shadowMaps.GetDSV(lightIndex);
}

ID3D11ShaderResourceView* SpotLightCollectionD3D11::GetShadowMapsSRV() const
{
	return shadowMaps.GetSRV();
}

ID3D11ShaderResourceView* SpotLightCollectionD3D11::GetLightBufferSRV() const
{
	return lightBuffer.GetSRV();
}

ID3D11Buffer* SpotLightCollectionD3D11::GetLightCameraConstantBuffer(UINT lightIndex) const
{
	if (lightIndex < shadowCameras.size())
	{
		return shadowCameras[lightIndex].GetConstantBuffer();
	}
	return nullptr;
}
