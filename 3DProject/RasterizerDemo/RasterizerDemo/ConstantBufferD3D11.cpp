#include "ConstantBufferD3D11.h"
#include <stdexcept>
#include <cstring>

ConstantBufferD3D11::~ConstantBufferD3D11()
{
    if (buffer)
    {
        buffer->Release();
        buffer = nullptr;
    }
}

ConstantBufferD3D11::ConstantBufferD3D11(ConstantBufferD3D11&& other) noexcept
{
    buffer = other.buffer;
    bufferSize = other.bufferSize;
    other.buffer = nullptr;
    other.bufferSize = 0;
}

ConstantBufferD3D11& ConstantBufferD3D11::operator=(ConstantBufferD3D11&& other) noexcept
{
    if (this != &other)
    {
        if (buffer) buffer->Release();
        buffer = other.buffer;
        bufferSize = other.bufferSize;
        other.buffer = nullptr;
        other.bufferSize = 0;
    }
    return *this;
}

ConstantBufferD3D11::ConstantBufferD3D11(ID3D11Device* device, size_t byteSize, void* initialData)
{
    Initialize(device, byteSize, initialData);
}

void ConstantBufferD3D11::Initialize(ID3D11Device* device, size_t byteSize, void* initialData)
{
    if (buffer) buffer->Release();

    bufferSize = (byteSize + 15) & ~15; // round up to 16 bytes

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = static_cast<UINT>(bufferSize);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA initData = {};
    D3D11_SUBRESOURCE_DATA* pInit = nullptr;
    if (initialData)
    {
        initData.pSysMem = initialData;
        pInit = &initData;
    }

    HRESULT hr = device->CreateBuffer(&desc, pInit, &buffer);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create constant buffer");
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
    std::memcpy(mapped.pData, data, bufferSize);
    context->Unmap(buffer, 0);
}
