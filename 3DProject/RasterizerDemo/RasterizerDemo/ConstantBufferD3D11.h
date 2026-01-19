#pragma once
#include <d3d11_4.h>
#include <cstddef>

class ConstantBufferD3D11
{
private:
    ID3D11Buffer* buffer = nullptr;

    // bufferSize = 16-byte aligned size (GPU buffer size)
    size_t bufferSize = 0;

    // dataSize = actual size of the data struct you pass in
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

    size_t GetSize() const;      // returns aligned GPU size
    size_t GetDataSize() const;  // returns logical struct size

    ID3D11Buffer* GetBuffer() const;

    void UpdateBuffer(ID3D11DeviceContext* context, const void* data);
};
