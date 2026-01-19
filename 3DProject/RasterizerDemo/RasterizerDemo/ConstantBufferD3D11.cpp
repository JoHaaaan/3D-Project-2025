#include "ConstantBufferD3D11.h"
#include <cstring> // memcpy, memset

ConstantBufferD3D11::ConstantBufferD3D11(ID3D11Device* device, size_t byteSize, void* initialData)
{
    Initialize(device, byteSize, initialData);
}

ConstantBufferD3D11::~ConstantBufferD3D11()
{
    if (buffer)
    {
        buffer->Release();
        buffer = nullptr;
    }
    bufferSize = 0;
    dataSize = 0;
}

ConstantBufferD3D11::ConstantBufferD3D11(ConstantBufferD3D11&& other) noexcept
    : buffer(other.buffer), bufferSize(other.bufferSize), dataSize(other.dataSize)
{
    other.buffer = nullptr;
    other.bufferSize = 0;
    other.dataSize = 0;
}

ConstantBufferD3D11& ConstantBufferD3D11::operator=(ConstantBufferD3D11&& other) noexcept
{
    if (this != &other)
    {
        if (buffer)
            buffer->Release();

        buffer = other.buffer;
        bufferSize = other.bufferSize;
        dataSize = other.dataSize;

        other.buffer = nullptr;
        other.bufferSize = 0;
        other.dataSize = 0;
    }
    return *this;
}

void ConstantBufferD3D11::Initialize(ID3D11Device* device, size_t byteSize, void* initialData)
{
    if (buffer)
    {
        buffer->Release();
        buffer = nullptr;
    }

    dataSize = byteSize;

    // Constant buffers must be 16-byte aligned in size
    UINT alignedSize = (static_cast<UINT>(byteSize) + 15u) & ~15u;
    bufferSize = alignedSize;

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = alignedSize;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    // Om initialData finns: skapa en temporär init-buffert som är aligned och nollpad:ad
    // så vi inte skickar oinitierad padding till GPU.
    if (initialData)
    {
        void* temp = ::operator new(alignedSize);
        std::memset(temp, 0, alignedSize);
        std::memcpy(temp, initialData, byteSize);

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = temp;

        HRESULT hr = device->CreateBuffer(&desc, &initData, &buffer);

        ::operator delete(temp);

        if (FAILED(hr))
        {
            buffer = nullptr;
            bufferSize = 0;
            dataSize = 0;
            return;
        }
    }
    else
    {
        HRESULT hr = device->CreateBuffer(&desc, nullptr, &buffer);
        if (FAILED(hr))
        {
            buffer = nullptr;
            bufferSize = 0;
            dataSize = 0;
            return;
        }
    }
}

size_t ConstantBufferD3D11::GetSize() const
{
    return bufferSize;
}

size_t ConstantBufferD3D11::GetDataSize() const
{
    return dataSize;
}

ID3D11Buffer* ConstantBufferD3D11::GetBuffer() const
{
    return buffer;
}

void ConstantBufferD3D11::UpdateBuffer(ID3D11DeviceContext* context, const void* data)
{
    if (!buffer || !context || !data)
        return;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr))
    {
        // Nollfyll hela bufferSize så padding alltid blir deterministisk
        std::memset(mapped.pData, 0, bufferSize);

        // Kopiera endast faktisk datastorlek (inte alignedSize!)
        std::memcpy(mapped.pData, data, dataSize);

        context->Unmap(buffer, 0);
    }
}
