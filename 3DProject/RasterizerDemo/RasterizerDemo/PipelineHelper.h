#pragma once

#include <array>
#include <string>
#include <d3d11.h>

struct SimpleVertex
{
	float pos[3];
	float nrm[3];
	float uv[2];

	SimpleVertex(const std::array<float, 3>& position, const std::array<float, 3>& normal, const std::array<float, 2>& uvCoords)
	{
		for (int i = 0; i < 3; ++i)
		{
			pos[i] = position[i];
			nrm[i] = normal[i];

		}
		uv[0] = uvCoords[0];
		uv[1] = uvCoords[1];
	}
};

bool SetupPipeline(ID3D11Device* device,
	ID3D11VertexShader*& vShader, ID3D11PixelShader*& pShader,
	std::string& outVertexShaderByteCode);
