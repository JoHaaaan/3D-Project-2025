#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include "TextureCubeD3D11.h"
#include "CameraD3D11.h"
#include "ConstantBufferD3D11.h"
#include "GameObject.h"
#include "SamplerD3D11.h"

class EnvironmentMapRenderer
{

private:
    void InitializeCameras(ID3D11Device* device);

    TextureCubeD3D11 m_cubeMap;
    CameraD3D11 m_cameras[6];
    ProjectionInfo m_projInfo;

    // Camera rotation values for each face (DirectX cube map orientation)
    // Order: +X (right), -X (left), +Y (up), -Y (down), +Z (forward), -Z (back)
    float m_upRotations[6] = {
      0.0f,        // +X face: no roll
   0.0f,         // -X face: no roll
        0.0f,               // +Y face: no roll
      DirectX::XM_PI,     // -Y face: 180° roll (flip upside down)
        0.0f,   // +Z face: no roll
        0.0f      // -Z face: no roll
    };
    float m_rightRotations[6] = {
        DirectX::XM_PIDIV2,     // +X face: 90° right
      -DirectX::XM_PIDIV2,    // -X face: -90° right
        0.0f,// +Y face: 0° (then pitch up)
   0.0f,     // -Y face: 0° (then pitch down)
        0.0f,          // +Z face: 0° (look forward)
        DirectX::XM_PI          // -Z face: 180° (look back)
    };

public:
    EnvironmentMapRenderer() = default;
    ~EnvironmentMapRenderer() = default;

    // Initialize the environment map renderer
    bool Initialize(ID3D11Device* device, UINT resolution = 512);

    // Render the environment map for a reflective object
    void RenderEnvironmentMap(
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
        ID3D11ShaderResourceView* fallbackTexture
    );

    // Get the cube map SRV for sampling in the reflection shader
    ID3D11ShaderResourceView* GetEnvironmentSRV() const { return m_cubeMap.GetSRV(); }


};
