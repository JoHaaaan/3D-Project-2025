#pragma once
#include <d3d11_4.h>
#include <DirectXMath.h>

struct Particle
{
	float position[3];
	float velocity[3];
	float lifetime;
	float MaxLifetime;
	float color;
	float size;
};