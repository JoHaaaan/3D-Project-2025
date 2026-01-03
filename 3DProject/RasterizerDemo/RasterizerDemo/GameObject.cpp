#include "GameObject.h"
#include "RenderHelper.h" // Needed for MatrixPair struct

using namespace DirectX;

// Local struct to match HLSL cbuffer padding requirements
struct MaterialPadding
{
    XMFLOAT3 ambient;
    float    padding1;
    XMFLOAT3 diffuse;
    float    padding2;
    XMFLOAT3 specular;
    float    specularPower;
};

GameObject::GameObject(const MeshD3D11* mesh)
    : m_mesh(mesh)
{
    // Default to identity matrix
    XMStoreFloat4x4(&m_worldMatrix, XMMatrixIdentity());
}

void GameObject::SetWorldMatrix(const DirectX::XMMATRIX& world)
{
    XMStoreFloat4x4(&m_worldMatrix, world);
}

DirectX::XMMATRIX GameObject::GetWorldMatrix() const
{
    return XMLoadFloat4x4(&m_worldMatrix);
}

DirectX::BoundingBox GameObject::GetWorldBoundingBox() const
{
    if (!m_mesh)
    {
        // Return empty bounding box if no mesh
        return DirectX::BoundingBox(DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT3(0, 0, 0));
    }

    // Get the local bounding box from the mesh
    DirectX::BoundingBox localBox = m_mesh->GetLocalBoundingBox();

    // Transform it using the object's world matrix
    DirectX::BoundingBox worldBox;
    localBox.Transform(worldBox, GetWorldMatrix());

    return worldBox;
}

void GameObject::Draw(ID3D11DeviceContext* context,
    ConstantBufferD3D11& matrixBuffer,
    ConstantBufferD3D11& materialBuffer,
    const DirectX::XMMATRIX& viewProjection,
    ID3D11ShaderResourceView* fallbackTexture)
{
    if (!m_mesh) return;

    // 1. Update Matrix Buffer (World + ViewProj)
    // Note: The shader expects Transposed matrices
    MatrixPair matrixData;
    XMMATRIX world = GetWorldMatrix();
    XMStoreFloat4x4(&matrixData.world, XMMatrixTranspose(world));
    XMStoreFloat4x4(&matrixData.viewProj, XMMatrixTranspose(viewProjection));

    matrixBuffer.UpdateBuffer(context, &matrixData);

    // 2. Bind Vertex/Index Buffers
    m_mesh->BindMeshBuffers(context);

    // 3. Render Submeshes
    for (size_t i = 0; i < m_mesh->GetNrOfSubMeshes(); ++i)
    {
        // a. Prepare Material Data with padding
        const auto& meshMat = m_mesh->GetMaterial(i);
        MaterialPadding matData;
        matData.ambient = meshMat.ambient;
        matData.padding1 = 0.0f;
        matData.diffuse = meshMat.diffuse;
        matData.padding2 = 0.0f;
        matData.specular = meshMat.specular;
        matData.specularPower = meshMat.specularPower;

        materialBuffer.UpdateBuffer(context, &matData);

        // b. Bind Texture (or fallback if the mesh doesn't have one)
        ID3D11ShaderResourceView* texture = m_mesh->GetDiffuseSRV(i);
        if (!texture) texture = fallbackTexture;

        context->PSSetShaderResources(0, 1, &texture);

        // c. Draw Call
        m_mesh->PerformSubMeshDrawCall(context, i);
    }
}