#include "SpotLightCollectionD3D11.h"

using namespace DirectX;

void SpotLightCollectionD3D11::Initialize(ID3D11Device* device, const SpotLightData& lightInfo)
{
    // 1. Clear existing data
    bufferData.clear();
    shadowCameras.clear();

    size_t numLights = lightInfo.perLightInfo.size();
    if (numLights == 0) return;

    bufferData.reserve(numLights);
    shadowCameras.reserve(numLights);

    // 2. Setup Cameras and Light Data
    for (const auto& info : lightInfo.perLightInfo)
    {
        // -- Setup Shadow Camera --
        ProjectionInfo proj = {};
        proj.fovAngleY = info.angle;
        proj.aspectRatio = 1.0f;
        proj.nearZ = info.projectionNearZ;
        proj.farZ = info.projectionFarZ;

        CameraD3D11 cam(device, proj, info.initialPosition);

        // Apply rotations
        cam.RotateRight(info.rotationY);
        cam.RotateForward(info.rotationX);

        shadowCameras.push_back(std::move(cam));

        // -- Setup Light Buffer Data --
        LightBuffer lb = {};
        lb.colour = info.colour;
        lb.angle = info.angle;

        // Get world vectors from camera
        lb.position = shadowCameras.back().GetPosition();
        lb.direction = shadowCameras.back().GetForward();

        // View-Projection Matrix for Shadows
        XMFLOAT4X4 vp = shadowCameras.back().GetViewProjectionMatrix();
        XMMATRIX vpMat = XMLoadFloat4x4(&vp);
        XMStoreFloat4x4(&lb.vpMatrix, XMMatrixTranspose(vpMat));

        bufferData.push_back(lb);
    }

    // 3. Initialize Structured Buffer (Light Data)
    lightBuffer.Initialize(device, sizeof(LightBuffer), numLights, bufferData.data(), true);

    // 4. Initialize Shadow Maps (Depth Buffer Array)
    UINT shadowDim = lightInfo.shadowMapInfo.textureDimension;
    if (shadowDim == 0) shadowDim = 1024;

    shadowMaps.Initialize(device, shadowDim, shadowDim, true, static_cast<UINT>(numLights));
}

void SpotLightCollectionD3D11::UpdateLightBuffers(ID3D11DeviceContext* context)
{
    if (!context || bufferData.empty()) return;

    for (size_t i = 0; i < shadowCameras.size(); ++i)
    {
        // 1. Update Camera Constant Buffer
        shadowCameras[i].UpdateInternalConstantBuffer(context);

        // 2. Update CPU-side LightBuffer data
        XMFLOAT4X4 vp = shadowCameras[i].GetViewProjectionMatrix();
        XMMATRIX vpMat = XMLoadFloat4x4(&vp);
        XMStoreFloat4x4(&bufferData[i].vpMatrix, XMMatrixTranspose(vpMat));

        bufferData[i].position = shadowCameras[i].GetPosition();
        bufferData[i].direction = shadowCameras[i].GetForward();
    }

    // 3. Upload to GPU Structured Buffer
    lightBuffer.UpdateBuffer(context, bufferData.data());
}

UINT SpotLightCollectionD3D11::GetNrOfLights() const
{
    return static_cast<UINT>(bufferData.size());
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
    if (lightIndex >= shadowCameras.size()) return nullptr;
    return shadowCameras[lightIndex].GetConstantBuffer();
}