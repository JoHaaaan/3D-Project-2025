#include "StructuredBufferD3D11.h"

StructuredBufferD3D11::StructuredBufferD3D11(ID3D11Device* device, UINT sizeOfElement,
	size_t nrOfElementsInBuffer, void* bufferData, bool dynamic)
	: elementSize(sizeOfElement), nrOfElements(nrOfElementsInBuffer)
{
	Initialize(device, sizeOfElement, nrOfElementsInBuffer, bufferData, dynamic);
}

StructuredBufferD3D11::~StructuredBufferD3D11()
{
	if (srv)
		srv->Release();
	if (buffer)
		buffer->Release();
}

void StructuredBufferD3D11::Initialize(ID3D11Device* device, UINT sizeOfElement,
	size_t nrOfElementsInBuffer, void* bufferData, bool dynamic)
{
	elementSize = sizeOfElement;
	nrOfElements = nrOfElementsInBuffer;

	// Buffer description
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.ByteWidth = elementSize * static_cast<UINT>(nrOfElements);
	bufferDesc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.StructureByteStride = elementSize;

	// Initialize with data if provided
	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = bufferData;

	device->CreateBuffer(&bufferDesc, bufferData ? &initData : nullptr, &buffer);

	// Create shader resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = static_cast<UINT>(nrOfElements);

	device->CreateShaderResourceView(buffer, &srvDesc, &srv);
}

void StructuredBufferD3D11::UpdateBuffer(ID3D11DeviceContext* context, void* data)
{
	D3D11_MAPPED_SUBRESOURCE mapped;
	context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, data, elementSize * nrOfElements);
	context->Unmap(buffer, 0);
}

UINT StructuredBufferD3D11::GetElementSize() const
{
	return elementSize;
}

size_t StructuredBufferD3D11::GetNrOfElements() const
{
	return nrOfElements;
}

ID3D11ShaderResourceView* StructuredBufferD3D11::GetSRV() const
{
	return srv;
}
