#pragma once
#include <d3d11_4.h>
#include <cstddef>

class ConstantBufferD3D11
{
private:
    ID3D11Buffer* buffer = nullptr;
    size_t bufferSize = 0;
    size_t dataSize = 0;

public:
    ConstantBufferD3D11() = default;
    ConstantBufferD3D11(ID3D11Device* device, size_t byteSize, void* initialData = nullptr);
    ~ConstantBufferD3D11();

    ConstantBufferD3D11(const ConstantBufferD3D11& other) = delete;
    ConstantBufferD3D11& operator=(const ConstantBufferD3D11& other) = delete;

    ConstantBufferD3D11(ConstantBufferD3D11&& other) noexcept;
    ConstantBufferD3D11& operator=(ConstantBufferD3D11&& other) noexcept;

    void Initialize(ID3D11Device* device, size_t byteSize, void* initialData = nullptr);

    size_t GetSize() const;
    size_t GetDataSize() const;

    ID3D11Buffer* GetBuffer() const;

    void UpdateBuffer(ID3D11DeviceContext* context, const void* data);
};
