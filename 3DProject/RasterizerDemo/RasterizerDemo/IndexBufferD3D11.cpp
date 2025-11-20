#include "IndexBufferD3D11.h"

IndexBufferD3D11::IndexBufferD3D11(ID3D11Device* device, size_t nrOfIndicesInBuffer, uint32_t* indexData)
{
	Initialize(device, nrOfIndicesInBuffer, indexData);
}

IndexBufferD3D11::~IndexBufferD3D11()
{
	if (buffer)
	{
		buffer->Release();
		buffer = nullptr;
	}
}

void IndexBufferD3D11::Initialize(ID3D11Device* device, size_t nrOfIndicesInBuffer, uint32_t* indexData)
{
	if (buffer)
	{
		buffer->Release();
		buffer = nullptr;
	}

	nrOfIndices = nrOfIndicesInBuffer;
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = static_cast<UINT>(nrOfIndicesInBuffer * sizeof(uint32_t));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = indexData;

	device->CreateBuffer(&desc, &initData, &buffer);
}

size_t IndexBufferD3D11::GetNrOfIndices() const
{
	return nrOfIndices;
}

ID3D11Buffer* IndexBufferD3D11::GetBuffer() const
{
	return buffer;
}
