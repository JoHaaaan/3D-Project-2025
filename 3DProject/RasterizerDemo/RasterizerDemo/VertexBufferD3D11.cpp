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