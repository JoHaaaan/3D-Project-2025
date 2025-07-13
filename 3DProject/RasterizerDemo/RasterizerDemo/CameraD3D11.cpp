#include "CameraD3D11.h"
#include <d3d11_4.h>
#include <DirectXMath.h>

using namespace DirectX;

// Helper struct matching the shader's camera constant buffer layout
namespace {
    struct CameraBufferType {
        XMFLOAT3 position;
        float    padding;  // Padding for 16-byte alignment
    };
}

CameraD3D11::CameraD3D11(ID3D11Device* device,
    const ProjectionInfo& projectionInfo,
    const XMFLOAT3& initialPosition)
    : position(initialPosition),
    forward(0.0f, 0.0f, 1.0f),
    right(1.0f, 0.0f, 0.0f),
    up(0.0f, 1.0f, 0.0f),
    projInfo(projectionInfo)
{
    // Initialize the GPU constant buffer with the starting position
    CameraBufferType cb{ position, 0.0f };
    cameraBuffer.Initialize(device, sizeof(CameraBufferType), &cb);
}

void CameraD3D11::Initialize(ID3D11Device* device,
    const ProjectionInfo& projectionInfo,
    const XMFLOAT3& initialPosition)
{
    position = initialPosition;
    forward = XMFLOAT3(0.0f, 0.0f, 1.0f);
    right = XMFLOAT3(1.0f, 0.0f, 0.0f);
    up = XMFLOAT3(0.0f, 1.0f, 0.0f);
    projInfo = projectionInfo;

    CameraBufferType cb{ position, 0.0f };
    cameraBuffer.Initialize(device, sizeof(CameraBufferType), &cb);
}

// Move the camera along a given direction vector
void CameraD3D11::MoveInDirection(float amount, const XMFLOAT3& direction)
{
    XMVECTOR pos = XMLoadFloat3(&position);
    XMVECTOR dir = XMLoadFloat3(&direction);
    pos = XMVectorAdd(pos, XMVectorScale(XMVector3Normalize(dir), amount));
    XMStoreFloat3(&position, pos);
}

void CameraD3D11::MoveForward(float amount)
{
    MoveInDirection(amount, forward);
}

void CameraD3D11::MoveRight(float amount)
{
    MoveInDirection(amount, right);
}

void CameraD3D11::MoveUp(float amount)
{
    MoveInDirection(amount, up);
}

// Rotate the camera around a given axis
void CameraD3D11::RotateAroundAxis(float amount, const XMFLOAT3& axis)
{
    XMVECTOR axisVec = XMLoadFloat3(&axis);
    XMVECTOR q = XMQuaternionRotationAxis(axisVec, amount);

    XMVECTOR f = XMVector3Rotate(XMLoadFloat3(&forward), q);
    XMVECTOR u = XMVector3Rotate(XMLoadFloat3(&up), q);
    XMVECTOR r = XMVector3Cross(f, u);

    XMStoreFloat3(&forward, XMVector3Normalize(f));
    XMStoreFloat3(&up, XMVector3Normalize(u));
    XMStoreFloat3(&right, XMVector3Normalize(r));
}

void CameraD3D11::RotateForward(float amount)
{
    RotateAroundAxis(amount, right);
}

void CameraD3D11::RotateRight(float amount)
{
    RotateAroundAxis(amount, up);
}

void CameraD3D11::RotateUp(float amount)
{
    RotateAroundAxis(amount, forward);
}

// Accessors
const XMFLOAT3& CameraD3D11::GetPosition() const { return position; }
const XMFLOAT3& CameraD3D11::GetForward()  const { return forward; }
const XMFLOAT3& CameraD3D11::GetRight()    const { return right; }
const XMFLOAT3& CameraD3D11::GetUp()       const { return up; }

// Update the GPU constant buffer with the latest camera position
void CameraD3D11::UpdateInternalConstantBuffer(ID3D11DeviceContext* context)
{
    CameraBufferType cb{ position, 0.0f };
    cameraBuffer.UpdateBuffer(context, &cb);
}

// Return the underlying D3D11 buffer for binding to the pipeline
ID3D11Buffer* CameraD3D11::GetConstantBuffer() const
{
    return cameraBuffer.GetBuffer();
}

// Compute and return the view-projection matrix
DirectX::XMFLOAT4X4 CameraD3D11::GetViewProjectionMatrix() const
{
    XMVECTOR posV = XMLoadFloat3(&position);
    XMVECTOR lookAtV = XMVectorAdd(posV, XMLoadFloat3(&forward));
    XMVECTOR upV = XMLoadFloat3(&up);

    XMMATRIX view = XMMatrixLookAtLH(posV, lookAtV, upV);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(
        projInfo.fovAngleY,
        projInfo.aspectRatio,
        projInfo.nearZ,
        projInfo.farZ
    );

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMFLOAT4X4  vp;
    XMStoreFloat4x4(&vp, viewProj);
    return vp;
}