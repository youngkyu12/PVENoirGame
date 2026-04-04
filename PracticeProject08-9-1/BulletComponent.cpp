#include "stdafx.h"
#include "BulletComponent.h"
#include "Object.h"
#include "ColliderComponent.h"

CBulletComponent::CBulletComponent(CGameObject* owner)
	: CComponentT<CBulletComponent>(owner)
{
}

void CBulletComponent::Activate(const XMFLOAT3& position, const XMFLOAT3& velocity, float lifeSec)
{
	CGameObject* owner = GetOwner();
	if ( !owner ) return;

	owner->SetPosition(position);

	m_velocity = velocity;
	m_lifeRemaining = ( lifeSec > 0.0f ) ? lifeSec : 0.0f;
	m_state = EState::Flying;

	if ( auto* collider = owner->GetComponent<CColliderComponent>() )
	{
		collider->SetCollisionEnabled(true);
	}
}

void CBulletComponent::Deactivate()
{
	CGameObject* owner = GetOwner();
	if ( owner )
	{
		owner->SetPosition(0.0f, -10000.0f, 0.0f);
	}

	if ( auto* collider = owner->GetComponent<CColliderComponent>() )
	{
		collider->SetCollisionEnabled(false);
	}

	m_state = EState::Inactive;
	m_lifeRemaining = 0.0f;
	m_velocity = { 0.0f, 0.0f, 0.0f };
}

void CBulletComponent::OnUpdate(float dt)
{
	CGameObject* owner = GetOwner();
	if ( !owner ) return;

	if ( m_state == EState::Inactive )
		return;

	if ( m_lifeRemaining > 0.0f )
	{
		m_lifeRemaining -= dt;
		if ( m_lifeRemaining <= 0.0f )
		{
			Deactivate();
			return;
		}
	}

	const XMFLOAT3 pos = owner->GetPosition();
	const XMFLOAT3 next =
	{
		pos.x + m_velocity.x * dt,
		pos.y + m_velocity.y * dt,
		pos.z + m_velocity.z * dt
	};

	owner->SetPosition(next);
}