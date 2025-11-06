#include "SamplerD3D11.h"

SamplerD3D11::SamplerD3D11(ID3D11Device* device, D3D11_TEXTURE_ADDRESS_MODE adressMode, std::optional<std::array<float, 4>> borderColour)
{

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

}

ID3D11SamplerState* SamplerD3D11::GetSamplerState() const
{
	return sampler;
}
