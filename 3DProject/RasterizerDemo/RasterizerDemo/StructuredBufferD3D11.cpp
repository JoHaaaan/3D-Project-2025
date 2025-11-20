#include "StructuredBufferD3D11.h"
#include <cstring>

StructuredBufferD3D11::StructuredBufferD3D11(ID3D11Device* device, UINT sizeOfElement,
    size_t nrOfElementsInBuffer, void* bufferData, bool dynamic)
{
    Initialize(device, sizeOfElement, nrOfElementsInBuffer, bufferData, dynamic);
}

StructuredBufferD3D11::~StructuredBufferD3D11()
{
    if (srv)
    {
        srv->Release();
        srv = nullptr;
    }
    if (buffer)
    {
        buffer->Release();
        buffer = nullptr;
    }
}

void StructuredBufferD3D11::Initialize(ID3D11Device* device, UINT sizeOfElement,
    size_t nrOfElementsInBuffer, void* bufferData, bool dynamic)
{
    if (srv)
    {
        srv->Release();
        srv = nullptr;
    }
    if (buffer)
    {
        buffer->Release();
        buffer = nullptr;
    }

    elementSize = sizeOfElement;
    nrOfElements = nrOfElementsInBuffer;

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = static_cast<UINT>(sizeOfElement * nrOfElementsInBuffer);
    desc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeOfElement;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = bufferData;

    HRESULT hr = device->CreateBuffer(&desc, bufferData ? &initData : nullptr, &buffer);
    if (FAILED(hr))
    {
        buffer = nullptr;
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = static_cast<UINT>(nrOfElementsInBuffer);

    hr = device->CreateShaderResourceView(buffer, &srvDesc, &srv);
    if (FAILED(hr))
    {
        srv = nullptr;
    }
}

void StructuredBufferD3D11::UpdateBuffer(ID3D11DeviceContext* context, void* data)
{
    if (!buffer || !data)
        return;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, data, elementSize * nrOfElements);
        context->Unmap(buffer, 0);
    }
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
