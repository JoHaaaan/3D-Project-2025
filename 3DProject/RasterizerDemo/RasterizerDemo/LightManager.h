#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include "RenderHelper.h"
#include "StructuredBufferD3D11.h"

class LightManager
{
public:
    LightManager() = default;
    ~LightManager() = default;

    // Initialize default lights (directional + spotlights)
    void InitializeDefaultLights(ID3D11Device* device);

    // Getters
    std::vector<LightData>& GetLights() { return m_lights; }
    const std::vector<LightData>& GetLights() const { return m_lights; }
    ID3D11ShaderResourceView* GetLightBufferSRV() const { return m_lightBuffer.GetSRV(); }
    size_t GetLightCount() const { return m_lights.size(); }

    // Get light view-projection matrix
    DirectX::XMMATRIX GetLightViewProj(size_t index) const;

private:
    void SetupDirectionalLight(LightData& light);
    void SetupSpotLight(LightData& light, const DirectX::XMFLOAT3& color,
        const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction);

    std::vector<LightData> m_lights;
    StructuredBufferD3D11 m_lightBuffer;
};
