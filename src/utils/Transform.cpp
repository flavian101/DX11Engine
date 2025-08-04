#include "Transform.h"

Transform::Transform(Graphics& gfx)
{
	vsBuffer.Initialize(gfx);
	m_Transform = DirectX::XMMatrixIdentity();
	m_Translation = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	m_Scale = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
	m_Rotation = DirectX::XMQuaternionIdentity();
}

void Transform::SetTranslation(const DirectX::XMFLOAT3& translation)
{
	m_Translation = DirectX::XMVectorSet(translation.x, translation.y, translation.z, 1.0f);
}

const DirectX::XMVECTOR& Transform::GetTranslation() const
{
	return m_Translation;
}

void Transform::SetScale(const DirectX::XMFLOAT3& scale)
{
	m_Scale = DirectX::XMVectorSet(scale.x, scale.y, scale.z, 1.0f);
}

const DirectX::XMVECTOR& Transform::GetScale() const
{
	return m_Scale;
}

void Transform::SetRotation(const DirectX::XMVECTOR& rotation)
{
	m_Rotation = rotation;
}

const DirectX::XMVECTOR& Transform::GetRotation() const
{
	return m_Rotation;
}

const DirectX::XMMATRIX& Transform::GetTransform() const
{
	return m_Transform;
}

void Transform::Bind(Graphics& gfx)
{
	m_Transform = DirectX::XMMatrixScalingFromVector(m_Scale) *
		DirectX::XMMatrixRotationQuaternion(m_Rotation) * 
		DirectX::XMMatrixTranslationFromVector(m_Translation);

	vsBuffer.data.WVP = XMMatrixTranspose(m_Transform * gfx.GetCamera().GetView() * gfx.GetCamera().GetProjection());
	vsBuffer.data.Model = XMMatrixTranspose(m_Transform);
	vsBuffer.Update(gfx);
	gfx.GetContext()->VSSetConstantBuffers(BindSlot::CB_Transform, 1, vsBuffer.GetAddressOf());

}

