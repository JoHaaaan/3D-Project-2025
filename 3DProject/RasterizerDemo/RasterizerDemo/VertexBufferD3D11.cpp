#include "VertexBufferD3D11.h"

VertexBufferD3D11::VertexBufferD3D11(ID3D11Device* device, UINT sizeOfVertex,
	UINT nrOfVerticesInBuffer, void* vertexData)
	: vertexSize(sizeOfVertex), nrOfVertices(nrOfVerticesInBuffer)
{
	Initialize(device, sizeOfVertex, nrOfVerticesInBuffer, vertexData);
}

VertexBufferD3D11::~VertexBufferD3D11()
{
	if (buffer)
		buffer->Release();
}

void VertexBufferD3D11::Initialize(ID3D11Device* device, UINT sizeOfVertex,
	UINT nrOfVerticesInBuffer, void* vertexData)
{
	vertexSize = sizeOfVertex;
	nrOfVertices = nrOfVerticesInBuffer;

	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.ByteWidth = vertexSize * nrOfVertices;
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = vertexData;

	device->CreateBuffer(&bufferDesc, &data, &buffer);
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
