#include "SamplerD3D11.h"

SamplerD3D11::SamplerD3D11(ID3D11Device* device, D3D11_TEXTURE_ADDRESS_MODE adressMode, std::optional<std::array<float, 4>> borderColour)
{
	Initialize(device, adressMode, borderColour);
}

SamplerD3D11::~SamplerD3D11()
{
	if (sampler)
	{
		sampler->Release();
		sampler = nullptr;
	}
}

void SamplerD3D11::Initialize(ID3D11Device* device, D3D11_TEXTURE_ADDRESS_MODE adressMode, std::optional<std::array<float, 4>> borderColour)
{
	if (sampler) {
		sampler->Release();
		sampler = nullptr;
	}

	D3D11_SAMPLER_DESC desc = {};
	desc.Filter = D3D11_FILTER_ANISOTROPIC;  // Changed from D3D11_FILTER_MIN_MAG_MIP_LINEAR
	desc.AddressU = adressMode;
	desc.AddressV = adressMode;
	desc.AddressW = adressMode;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	desc.MinLOD = 0.0f;
	desc.MaxLOD = D3D11_FLOAT32_MAX;
	desc.MaxAnisotropy = 16;  // Added: Maximum anisotropic filtering samples

	if (borderColour.has_value()) {
		for (int i = 0; i < 4; ++i) {
			desc.BorderColor[i] = (*borderColour)[i];
		}
	}

	HRESULT hr = device->CreateSamplerState(&desc, &sampler);
	if (FAILED(hr))
	{
		std::printf("CreateSamplerState failed. hr=0x%08X\n", (unsigned)hr);
	}
}
ID3D11SamplerState* SamplerD3D11::GetSamplerState() const
{
	return sampler;
}
