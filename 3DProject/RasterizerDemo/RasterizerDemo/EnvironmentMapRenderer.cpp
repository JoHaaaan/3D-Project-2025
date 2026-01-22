#include "EnvironmentMapRenderer.h"
#include "RenderHelper.h"
#include <Windows.h>

using namespace DirectX;

bool EnvironmentMapRenderer::Initialize(ID3D11Device* device, UINT resolution)
{
    if (!m_cubeMap.Initialize(device, resolution, resolution))
    {
        return false;
    }

    m_projInfo.fovAngleY = XM_PIDIV2;
    m_projInfo.aspectRatio = 1.0f;
    m_projInfo.nearZ = 0.1f;
    m_projInfo.farZ = 100.0f;

    InitializeCameras(device);

    return true;
}

void EnvironmentMapRenderer::InitializeCameras(ID3D11Device* device)
{
    for (int i = 0; i < 6; ++i)
    {
        m_cameras[i].Initialize(device, m_projInfo, XMFLOAT3(0.0f, 0.0f, 0.0f));
   
        m_cameras[i].RotateRight(m_rightRotations[i]);
      
        if (i == 2)
        {
            m_cameras[i].RotateForward(-XM_PIDIV2);
        }
        else if (i == 3)
        {
            m_cameras[i].RotateForward(XM_PIDIV2);
        }
        
        m_cameras[i].RotateUp(m_upRotations[i]);
    }
}

void EnvironmentMapRenderer::RenderEnvironmentMap(
    ID3D11DeviceContext* context,
    ID3D11Device* device,
    const DirectX::XMFLOAT3& objectPosition,
    std::vector<GameObject>& gameObjects,
    size_t reflectiveObjectIndex,
    ID3D11VertexShader* vertexShader,
    ID3D11PixelShader* cubeMapPixelShader,
    ID3D11InputLayout* inputLayout,
    ConstantBufferD3D11& constantBuffer,
    ConstantBufferD3D11& materialBuffer,
    SamplerD3D11& sampler,
    ID3D11ShaderResourceView* fallbackTexture)
{
    for (int i = 0; i < 6; ++i)
    {
        m_cameras[i].SetPosition(objectPosition);
    }

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
        ID3D11RenderTargetView* cubeRTV = m_cubeMap.GetRTV(faceIndex);
        ID3D11DepthStencilView* cubeDSV = m_cubeMap.GetDSV();
        context->OMSetRenderTargets(1, &cubeRTV, cubeDSV);

        float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        m_cubeMap.ClearFace(context, faceIndex, clearColor);

        D3D11_VIEWPORT cubeViewport = m_cubeMap.GetViewport();
        context->RSSetViewports(1, &cubeViewport);

        context->IASetInputLayout(inputLayout);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertexShader, nullptr, 0);
        context->HSSetShader(nullptr, nullptr, 0);
        context->DSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(cubeMapPixelShader, nullptr, 0);

        ID3D11Buffer* cb0 = constantBuffer.GetBuffer();
        context->VSSetConstantBuffers(0, 1, &cb0);

        ID3D11Buffer* matCB = materialBuffer.GetBuffer();
        context->PSSetConstantBuffers(2, 1, &matCB);

        m_cameras[faceIndex].UpdateInternalConstantBuffer(context);
        ID3D11Buffer* cameraCB = m_cameras[faceIndex].GetConstantBuffer();
        context->PSSetConstantBuffers(3, 1, &cameraCB);

        ID3D11SamplerState* samplerPtr = sampler.GetSamplerState();
        context->PSSetSamplers(0, 1, &samplerPtr);

        XMFLOAT4X4 cubeVP = m_cameras[faceIndex].GetViewProjectionMatrix();
        XMMATRIX cubeViewProj = XMLoadFloat4x4(&cubeVP);

        for (size_t objIdx = 0; objIdx < gameObjects.size(); ++objIdx)
        {
            if (objIdx == reflectiveObjectIndex)
                continue;

            gameObjects[objIdx].Draw(context, constantBuffer, materialBuffer, cubeViewProj, fallbackTexture);

            ID3D11ShaderResourceView* nullSRV = nullptr;
            context->PSSetShaderResources(0, 1, &nullSRV);
        }
    }

    ID3D11RenderTargetView* nullRTV = nullptr;
    context->OMSetRenderTargets(1, &nullRTV, nullptr);
}
