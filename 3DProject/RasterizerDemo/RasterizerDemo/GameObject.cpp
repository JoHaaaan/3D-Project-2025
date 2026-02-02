#include "GameObject.h"
#include "CommonStructures.h"

using namespace DirectX;

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
		return DirectX::BoundingBox(DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT3(0, 0, 0));
	}

	DirectX::BoundingBox localBox = m_mesh->GetLocalBoundingBox();

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

	MatrixPair matrixData;
	XMMATRIX world = GetWorldMatrix();
	XMStoreFloat4x4(&matrixData.world, XMMatrixTranspose(world));
	XMStoreFloat4x4(&matrixData.viewProj, XMMatrixTranspose(viewProjection));

	matrixBuffer.UpdateBuffer(context, &matrixData);

	m_mesh->BindMeshBuffers(context);

	for (size_t i = 0; i < m_mesh->GetNrOfSubMeshes(); ++i)
	{
		const auto& meshMat = m_mesh->GetMaterial(i);
		MaterialPadding matData;
		matData.ambient = meshMat.ambient;
		matData.padding1 = 0.0f;
		matData.diffuse = meshMat.diffuse;
		matData.padding2 = 0.0f;
		matData.specular = meshMat.specular;
		matData.specularPower = meshMat.specularPower;

		materialBuffer.UpdateBuffer(context, &matData);

		ID3D11ShaderResourceView* texture = m_mesh->GetDiffuseSRV(i);
		if (!texture) texture = fallbackTexture;

		context->PSSetShaderResources(0, 1, &texture);

		m_mesh->PerformSubMeshDrawCall(context, i);
	}
}