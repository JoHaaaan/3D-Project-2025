#include "CameraD3D11.h"

using namespace DirectX;

struct CameraCB
{
    XMFLOAT4X4 viewProjection;
};

CameraD3D11::CameraD3D11(ID3D11Device* device, const ProjectionInfo& projectionInfo, const XMFLOAT3& initialPosition)
{
    Initialize(device, projectionInfo, initialPosition);
}

void CameraD3D11::Initialize(ID3D11Device* device, const ProjectionInfo& projectionInfo, const XMFLOAT3& initialPosition)
{
    projInfo = projectionInfo;
    position = initialPosition;
    forward = XMFLOAT3(0.0f, 0.0f, 1.0f);
    right = XMFLOAT3(1.0f, 0.0f, 0.0f);
    up = XMFLOAT3(0.0f, 1.0f, 0.0f);
    cameraBuffer.Initialize(device, sizeof(CameraCB));
}

void CameraD3D11::MoveInDirection(float amount, const XMFLOAT3& direction)
{
    XMVECTOR pos = XMLoadFloat3(&position);
    XMVECTOR dir = XMLoadFloat3(&direction);
    pos = XMVectorAdd(pos, XMVectorScale(dir, amount));
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

void CameraD3D11::RotateAroundAxis(float amount, const XMFLOAT3& axis)
{
    XMVECTOR axisVec = XMLoadFloat3(&axis);
    XMVECTOR rotation = XMQuaternionRotationAxis(axisVec, amount);

    XMVECTOR f = XMLoadFloat3(&forward);
    XMVECTOR r = XMLoadFloat3(&right);
    XMVECTOR u = XMLoadFloat3(&up);

    f = XMVector3Rotate(f, rotation);
    r = XMVector3Rotate(r, rotation);
    u = XMVector3Rotate(u, rotation);

    XMStoreFloat3(&forward, f);
    XMStoreFloat3(&right, r);
    XMStoreFloat3(&up, u);
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

const XMFLOAT3& CameraD3D11::GetPosition() const
{
    return position;
}

const XMFLOAT3& CameraD3D11::GetForward() const
{
    return forward;
}

const XMFLOAT3& CameraD3D11::GetRight() const
{
    return right;
}

const XMFLOAT3& CameraD3D11::GetUp() const
{
    return up;
}

void CameraD3D11::UpdateInternalConstantBuffer(ID3D11DeviceContext* context)
{
    XMVECTOR eye = XMLoadFloat3(&position);
    XMVECTOR target = XMVectorAdd(eye, XMLoadFloat3(&forward));
    XMVECTOR upVec = XMLoadFloat3(&up);
    XMMATRIX view = XMMatrixLookAtLH(eye, target, upVec);

    XMMATRIX proj = XMMatrixPerspectiveFovLH(
        projInfo.fovAngleY,
        projInfo.aspectRatio,
        projInfo.nearZ,
        projInfo.farZ
    );

    XMMATRIX vp = XMMatrixMultiply(view, proj);

    CameraCB cbData;
    XMStoreFloat4x4(&cbData.viewProjection, vp);
    cameraBuffer.UpdateBuffer(context, &cbData);
}

ID3D11Buffer* CameraD3D11::GetConstantBuffer() const
{
    return cameraBuffer.GetBuffer();
}

XMFLOAT4X4 CameraD3D11::GetViewProjectionMatrix() const
{
    XMVECTOR eye = XMLoadFloat3(&position);
    XMVECTOR target = XMVectorAdd(eye, XMLoadFloat3(&forward));
    XMVECTOR upVec = XMLoadFloat3(&up);

    XMMATRIX view = XMMatrixLookAtLH(eye, target, upVec);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(
        projInfo.fovAngleY,
        projInfo.aspectRatio,
        projInfo.nearZ,
        projInfo.farZ
    );

    XMFLOAT4X4 result;
    XMStoreFloat4x4(&result, XMMatrixMultiply(view, proj));
    return result;
}
