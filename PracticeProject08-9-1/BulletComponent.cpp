#include "stdafx.h"
#include "BulletComponent.h"
#include "Object.h"
#include "ColliderComponent.h"

namespace
{
	enum : uint32_t
	{
		kCollisionLayerPlayer = 0,
		kCollisionLayerMonster = 1,
		kCollisionLayerPlayerWeapon = 3,
		kCollisionLayerMonsterWeapon = 4
	};

	static constexpr uint32_t CollisionBit(uint32_t layer)
	{
		return ( 1u << layer );
	}

	static constexpr float kBulletGravityY = -3.0f;
}

CBulletComponent::CBulletComponent(CGameObject* owner)
	: CComponentT<CBulletComponent>(owner)
{
}

XMFLOAT3 CBulletComponent::NormalizeSafe(const XMFLOAT3& v)
{
	XMVECTOR vec = XMLoadFloat3(&v);
	const float lenSq = XMVectorGetX(XMVector3LengthSq(vec));

	if ( lenSq < 1e-8f )
		return XMFLOAT3(0.0f, 0.0f, 1.0f);

	XMFLOAT3 out{};
	XMStoreFloat3(&out, XMVector3Normalize(vec));
	return out;
}

XMFLOAT3 CBulletComponent::GetForwardFromObject(const CGameObject* obj)
{
	if ( !obj )
		return XMFLOAT3(0.0f, 0.0f, 1.0f);

	const XMFLOAT4X4& W = obj->GetWorldMatrix();
	return NormalizeSafe(XMFLOAT3(W._31, W._32, W._33));
}

void CBulletComponent::ApplyProjectileColliderProfile()
{
	CGameObject* owner = GetOwner();
	if ( !owner ) return;

	auto* collider = owner->GetComponent<CColliderComponent>();
	if ( !collider ) return;

	if ( m_firedByPlayer )
	{
		collider->SetLayer(kCollisionLayerPlayerWeapon);
		collider->SetMask(CollisionBit(kCollisionLayerMonster));
	}
	else
	{
		collider->SetLayer(kCollisionLayerMonsterWeapon);
		collider->SetMask(CollisionBit(kCollisionLayerPlayer));
	}
}

bool CBulletComponent::FireFromObjects(
	CGameObject* spawnSource,
	CGameObject* directionSource,
	float speed,
	float lifeSec,
	bool firedByPlayer)
{
	CGameObject* actualSpawnSource = spawnSource ? spawnSource : directionSource;
	CGameObject* actualDirectionSource = directionSource ? directionSource : spawnSource;

	if ( !actualSpawnSource ) return false;
	if ( !actualDirectionSource ) return false;

	const XMFLOAT3 startPos = actualSpawnSource->GetPosition();

	XMFLOAT3 dir = GetForwardFromObject(actualDirectionSource);

	// 발사 시작 방향은 항상 지면에 수평이 되도록 y 성분 제거.
	dir.y = 0.0f;
	dir = NormalizeSafe(dir);

	const XMFLOAT3 velocity =
	{
		dir.x * speed,
		0.0f,
		dir.z * speed
	};

	Activate(startPos, velocity, lifeSec, firedByPlayer);
	return IsActive();
}

void CBulletComponent::Activate(const XMFLOAT3& position, const XMFLOAT3& velocity, float lifeSec, bool firedByPlayer)
{
	CGameObject* owner = GetOwner();
	if ( !owner ) return;

	owner->SetPosition(position);

	m_velocity = velocity;
	m_velocity.y = 0.0f;

	m_lifeRemaining = ( lifeSec > 0.0f ) ? lifeSec : 0.0f;
	m_firedByPlayer = firedByPlayer;
	m_state = EState::Flying;

	if ( auto* tr = owner->GetComponent<CTransformComponent>() )
	{
		tr->SetLookDirection(NormalizeSafe(m_velocity));
	}

	ApplyProjectileColliderProfile();

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

		if ( auto* collider = owner->GetComponent<CColliderComponent>() )
		{
			collider->SetCollisionEnabled(false);
		}
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

	m_velocity.y += kBulletGravityY * dt;

	const XMFLOAT3 pos = owner->GetPosition();
	const XMFLOAT3 next =
	{
		pos.x + m_velocity.x * dt,
		pos.y + m_velocity.y * dt,
		pos.z + m_velocity.z * dt
	};

	owner->SetPosition(next);
}