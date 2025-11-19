#include "VertexBufferD3D11.h"

VertexBufferD3D11::VertexBufferD3D11(ID3D11Device* device, UINT sizeOfVertex, UINT nrOfVerticesInBuffer, void* vertexData)
{
	Initialize(device, sizeOfVertex, nrOfVerticesInBuffer, vertexData);

}

VertexBufferD3D11::~VertexBufferD3D11()
{
	if (buffer)
	{
		buffer->Release();
		buffer = nullptr;
	}

}

void VertexBufferD3D11::Initialize(ID3D11Device* device, UINT sizeOfVertex, UINT nrOfVerticesInBuffer, void* vertexData)
{
	if (buffer)
	{
		buffer->Release();
		buffer = nullptr;
	}

	vertexSize = sizeOfVertex;
	nrOfVertices = nrOfVerticesInBuffer;

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = sizeOfVertex * nrOfVerticesInBuffer;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = vertexData;

	device->CreateBuffer(
		&desc,
		vertexData ? &initData : nullptr,
		&buffer
	);
}

UINT VertexBufferD3D11::GetNrOfVertices() const
{
	return nrOfVertices;
}

UINT VertexBufferD3D11::GetVertexSize() const
{
	return vertexSize;
}

ID3D11Buffer* VertexBufferD3D11::GetBuffer() const 
{
	return buffer;
}