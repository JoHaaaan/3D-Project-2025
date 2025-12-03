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
        // The FOV should match the spotlight angle (x2 often, but x1 covers the radius if angle is full cone)
        // Usually spotlight angle is half-angle, so FOV = angle * 2. 
        // Let's assume input 'angle' is the full FOV for simplicity, or adjust as needed.
        ProjectionInfo proj = {};
        proj.fovAngleY = info.angle;
        proj.aspectRatio = 1.0f; // Shadow maps are square
        proj.nearZ = info.projectionNearZ;
        proj.farZ = info.projectionFarZ;

        CameraD3D11 cam(device, proj, info.initialPosition);

        // Apply rotations
        // RotateRight = Yaw (around Y), RotateForward = Pitch (around X)
        cam.RotateRight(info.rotationY);
        cam.RotateForward(info.rotationX);

        shadowCameras.push_back(std::move(cam));

        // -- Setup Light Buffer Data --
        LightBuffer lb = {};
        lb.colour = info.colour;
        lb.angle = info.angle; // Passed to shader for spotlight cone calc

        // Get world vectors from camera
        lb.position = shadowCameras.back().GetPosition();
        lb.direction = shadowCameras.back().GetForward();

        // View-Projection Matrix for Shadows
        // We transpose it for HLSL (column-major preference)
        XMFLOAT4X4 vp = shadowCameras.back().GetViewProjectionMatrix();
        XMMATRIX vpMat = XMLoadFloat4x4(&vp);
        XMStoreFloat4x4(&lb.vpMatrix, XMMatrixTranspose(vpMat));

        bufferData.push_back(lb);
    }

    // 3. Initialize Structured Buffer (Light Data)
    // Dynamic = true so we can update lights if they move
    lightBuffer.Initialize(device, sizeof(LightBuffer), numLights, bufferData.data(), true);

    // 4. Initialize Shadow Maps (Depth Buffer Array)
    // We create ONE depth buffer resource that is an array of textures.
    // hasSRV = true (so we can read it in the Compute Shader)
    // arraySize = numLights
    UINT shadowDim = lightInfo.shadowMapInfo.textureDimension;
    if (shadowDim == 0) shadowDim = 1024; // Default safety

    shadowMaps.Initialize(device, shadowDim, shadowDim, true, static_cast<UINT>(numLights));
}

void SpotLightCollectionD3D11::UpdateLightBuffers(ID3D11DeviceContext* context)
{
    if (!context || bufferData.empty()) return;

    for (size_t i = 0; i < shadowCameras.size(); ++i)
    {
        // 1. Update Camera Constant Buffer (for the Vertex Shader in Shadow Pass)
        shadowCameras[i].UpdateInternalConstantBuffer(context);

        // 2. Update CPU-side LightBuffer data (for the Compute Shader)
        // If lights moved, we need fresh matrices
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