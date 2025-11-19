#include "IndexBufferD3D11.h"

IndexBufferD3D11::IndexBufferD3D11(ID3D11Device* device, size_t nrOfIndicesInBuffer, uint32_t* indexData)
{
}

IndexBufferD3D11::~IndexBufferD3D11()
{
}

void IndexBufferD3D11::Initialize(ID3D11Device* device, size_t nrOfIndicesInBuffer, uint32_t* indexData)
{
}

size_t IndexBufferD3D11::GetNrOfIndices() const
{
	return size_t();
}

ID3D11Buffer* IndexBufferD3D11::GetBuffer() const
{
	return nullptr;
}
