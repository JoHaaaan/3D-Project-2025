#include "ConstantBufferD3D11.h"

ConstantBufferD3D11::ConstantBufferD3D11(ID3D11Device* device, size_t byteSize, void* initialData)
{
	Initialize(device, byteSize, initialData);
}

ConstantBufferD3D11::~ConstantBufferD3D11()
{
	if (buffer) buffer->Release();
}

ConstantBufferD3D11::ConstantBufferD3D11(ConstantBufferD3D11&& other) noexcept
	: buffer(other.buffer), bufferSize(other.bufferSize)
{
	other.buffer = nullptr;
	other.bufferSize = 0;
}

ConstantBufferD3D11& ConstantBufferD3D11::operator=(ConstantBufferD3D11&& other) noexcept
{
	if (this != &other) {
		if (buffer) buffer->Release();
		buffer = other.buffer;
		bufferSize = other.bufferSize;
		other.buffer = nullptr;
		other.bufferSize = 0;
	}
	return *this;
}

void ConstantBufferD3D11::Initialize(ID3D11Device* device, size_t byteSize, void* initialData)
{
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = static_cast<UINT>(byteSize);
	desc.Usage = initialData ? D3D11_USAGE_DEFAULT : D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = initialData ? 0 : D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = initialData;

	device->CreateBuffer(
		&desc,
		initialData ? &initData : nullptr,
		&buffer
	);

	bufferSize = byteSize;
}

size_t ConstantBufferD3D11::GetSize() const
{
	return bufferSize;
}

ID3D11Buffer* ConstantBufferD3D11::GetBuffer() const
{
	return buffer;
}

void ConstantBufferD3D11::UpdateBuffer(ID3D11DeviceContext* context, void* data)
{
	D3D11_MAPPED_SUBRESOURCE mapped;
	context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, data, bufferSize);
	context->Unmap(buffer, 0);
}
