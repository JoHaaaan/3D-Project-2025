#pragma once

#include <d3d11_4.h>

#include <vector>

class DepthBufferD3D11
{
private:

	// Underlying depth-stencil texture (typeless when SRV is enabled)
	ID3D11Texture2D* texture = nullptr;

	// One DSV per slice when using a texture array
	std::vector<ID3D11DepthStencilView*> depthStencilViews;

	// Optional SRV for sampling depth in shaders, only created only if hasSRV=true
	ID3D11ShaderResourceView* srv = nullptr;

public:
	DepthBufferD3D11() = default;
	DepthBufferD3D11(ID3D11Device* device, UINT width, UINT height, bool hasSRV = false);
	~DepthBufferD3D11();

	// Disable copy/move to prevent shallow sharing of COM pointers
	DepthBufferD3D11(const DepthBufferD3D11& other) = delete;
	DepthBufferD3D11& operator=(const DepthBufferD3D11& other) = delete;
	DepthBufferD3D11(DepthBufferD3D11&& other) = delete;
	DepthBufferD3D11& operator=(DepthBufferD3D11&& other) = delete;

	//Create or recreate the depth buffer(arraysize > 1 for texture arrays)
	void Initialize(ID3D11Device* device, UINT width, UINT height,
		bool hasSRV = false, UINT arraySize = 1);

	// Depth-stencil view for a specific slice
	ID3D11DepthStencilView* GetDSV(UINT arrayIndex) const;

	// Shader resource view for sampling depth
	ID3D11ShaderResourceView* GetSRV() const;
};