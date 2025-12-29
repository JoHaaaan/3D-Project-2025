#include "ConstantBufferD3D11.h"
#include "VertexBufferD3D11.h"
#include <Windows.h>
#include <stdio.h>

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
<<<<<<< HEAD
	if (buffer) 
	{ 
		buffer->Release(); buffer = nullptr; 
	}

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = static_cast<UINT>(byteSize);
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
=======
    // Release existing buffer if reinitializing
    if (buffer)
    {
        buffer->Release();
        buffer = nullptr;
    }
>>>>>>> e46f734fba3d5e3b4c05d590fef09318c65a38d4

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = static_cast<UINT>(byteSize);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = initialData;

    HRESULT hr = device->CreateBuffer(
   &desc,
initialData ? &initData : nullptr,
        &buffer
    );

  if (FAILED(hr))
    {
        char debugMsg[256];
        sprintf_s(debugMsg, "ERROR: Failed to create constant buffer! HRESULT: 0x%08X\n", hr);
        OutputDebugStringA(debugMsg);
        buffer = nullptr;
     bufferSize = 0;
        return;
    }

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
    // Add null/validity checks
    if (!buffer)
    {
        OutputDebugStringA("ERROR: UpdateBuffer - buffer is NULL!\n");
        return;
    }

    if (!context)
    {
 OutputDebugStringA("ERROR: UpdateBuffer - context is NULL!\n");
        return;
    }

    if (!data)
    {
        OutputDebugStringA("ERROR: UpdateBuffer - data is NULL!\n");
  return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
  HRESULT hr = context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

    if (FAILED(hr))
    {
        char debugMsg[256];
        sprintf_s(debugMsg, "ERROR: Failed to map constant buffer! HRESULT: 0x%08X\n", hr);
    OutputDebugStringA(debugMsg);
        return;
    }

    memcpy(mapped.pData, data, bufferSize);
    context->Unmap(buffer, 0);
}
