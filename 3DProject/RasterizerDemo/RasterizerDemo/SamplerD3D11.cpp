#include "SamplerD3D11.h"

SamplerD3D11::SamplerD3D11(ID3D11Device* device, D3D11_TEXTURE_ADDRESS_MODE adressMode,
	std::optional<std::array<float, 4>> borderColour)
{
	Initialize(device, adressMode, borderColour);
}

SamplerD3D11::~SamplerD3D11()
{
	if (sampler)
		sampler->Release();
}

void SamplerD3D11::Initialize(ID3D11Device* device, D3D11_TEXTURE_ADDRESS_MODE adressMode,
	std::optional<std::array<float, 4>> borderColour)
{
	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = adressMode;
	samplerDesc.AddressV = adressMode;
	samplerDesc.AddressW = adressMode;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	
	if (borderColour.has_value())
	{
		samplerDesc.BorderColor[0] = borderColour.value()[0];
		samplerDesc.BorderColor[1] = borderColour.value()[1];
		samplerDesc.BorderColor[2] = borderColour.value()[2];
		samplerDesc.BorderColor[3] = borderColour.value()[3];
	}
	else
	{
		samplerDesc.BorderColor[0] = 0.0f;
		samplerDesc.BorderColor[1] = 0.0f;
		samplerDesc.BorderColor[2] = 0.0f;
		samplerDesc.BorderColor[3] = 0.0f;
	}
	
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	device->CreateSamplerState(&samplerDesc, &sampler);
}

ID3D11SamplerState* SamplerD3D11::GetSamplerState() const
{
	return sampler;
}
