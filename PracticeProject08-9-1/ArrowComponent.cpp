#include "stdafx.h"
#include "ArrowComponent.h"
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

	static constexpr float kArrowGravityY = -2.5f;
}

CArrowComponent::CArrowComponent(CGameObject* owner)
    : CComponentT<CArrowComponent>(owner)
{
}

XMFLOAT3 CArrowComponent::NormalizeSafe(const XMFLOAT3& v)
{
    XMVECTOR vv = XMLoadFloat3(&v);
    const float lenSq = XMVectorGetX(XMVector3LengthSq(vv));
    if (lenSq < 1e-8f)
        return XMFLOAT3(0.0f, 0.0f, 1.0f);

    vv = XMVector3Normalize(vv);

    XMFLOAT3 out{};
    XMStoreFloat3(&out, vv);
    return out;
}

XMFLOAT3 CArrowComponent::GetForwardFromObject(const CGameObject* obj)
{
    if (!obj) return XMFLOAT3(0.0f, 0.0f, 1.0f);

    const XMFLOAT4X4& W = obj->GetWorldMatrix();
    return NormalizeSafe(XMFLOAT3(W._31, W._32, W._33));
}

void CArrowComponent::ApplyProjectileColliderProfile()
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

XMFLOAT3 CArrowComponent::BuildLaunchVelocity(float speed) const
{
	const CGameObject* source = m_directionSource ? m_directionSource : GetOwner();

	XMFLOAT3 dir = GetForwardFromObject(source);

	// 발사 시작 방향은 항상 지면에 수평이 되도록 y 성분 제거.
	dir.y = 0.0f;
	dir = NormalizeSafe(dir);

	return XMFLOAT3(dir.x * speed, 0.0f, dir.z * speed);
}

void CArrowComponent::Activate(const XMFLOAT3& position, const XMFLOAT3& velocity, float lifeSec, bool firedByPlayer) 
{
	CGameObject* owner = GetOwner();
	if ( !owner ) return;

	owner->SetPosition(position);

	m_bowObject = nullptr;
	m_directionSource = nullptr;
	m_pullBackDistance = 0.0f;

	m_velocity = velocity;
	m_velocity.y = 0.0f;

	m_lifeRemaining = ( lifeSec > 0.0f ) ? lifeSec : 0.0f;
	m_enableCollisionOnLaunch = true;
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

void CArrowComponent::Prepare(
	CGameObject* bowObject,
	CGameObject* directionSource,
	float pullBackDistance,
	bool firedByPlayer,
	bool enableCollisionOnLaunch)
{
    CGameObject* owner = GetOwner();
    if (!owner || !bowObject) return;

    m_bowObject = bowObject;
    m_directionSource = directionSource;
    m_pullBackDistance = (pullBackDistance > 0.0f) ? pullBackDistance : 0.0f;

	m_velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_lifeRemaining = 0.0f;
	m_enableCollisionOnLaunch = enableCollisionOnLaunch;
	m_firedByPlayer = firedByPlayer;
	m_state = EState::Prepared;

	ApplyProjectileColliderProfile();

	if ( auto* collider = owner->GetComponent<CColliderComponent>() )
	{
		collider->SetCollisionEnabled(false);
	}

	OnUpdate(0.0f); // 즉시 스냅
}

bool CArrowComponent::LaunchPrepared(float speed, float lifeSec, bool enableCollision)
{
	if ( m_state != EState::Prepared )
		return false;

	const XMFLOAT3 velocity = BuildLaunchVelocity(speed);
	Launch(velocity, lifeSec, enableCollision);
	return ( m_state == EState::Flying );
}

void CArrowComponent::Launch(const XMFLOAT3& velocity, float lifeSec, bool enableCollision) 
{
    if (m_state != EState::Prepared)
        return;

    m_bowObject = nullptr;
    m_directionSource = nullptr;
    m_pullBackDistance = 0.0f;

	m_velocity = velocity;
	m_velocity.y = 0.0f;

	m_lifeRemaining = ( lifeSec > 0.0f ) ? lifeSec : 0.0f;
	m_state = EState::Flying;

	CGameObject* owner = GetOwner();

	if ( owner )
	{
		if ( auto* tr = owner->GetComponent<CTransformComponent>() )
		{
			tr->SetLookDirection(NormalizeSafe(m_velocity));
		}

		ApplyProjectileColliderProfile();

		if ( auto* collider = owner->GetComponent<CColliderComponent>() )
		{
			collider->SetCollisionEnabled(enableCollision && m_enableCollisionOnLaunch);
		}
	}
}

void CArrowComponent::Deactivate()
{
    CGameObject* owner = GetOwner();
    if (owner)
    {
        owner->SetPosition(0.0f, -10000.0f, 0.0f);
    }

	if ( owner )
	{
		if ( auto* collider = owner->GetComponent<CColliderComponent>() )
		{
			collider->SetCollisionEnabled(false);
		}
	}

	m_state = EState::Inactive;
	m_lifeRemaining = 0.0f;
	m_velocity = { 0.0f, 0.0f, 0.0f };
	m_bowObject = nullptr;
	m_directionSource = nullptr;
	m_pullBackDistance = 0.0f;
	m_enableCollisionOnLaunch = true;
}

void CArrowComponent::OnUpdate(float dt)
{
    CGameObject* owner = GetOwner();
    if (!owner) return;

    if (m_state == EState::Inactive)
        return;

    if (m_state == EState::Prepared)
    {
        if (!m_bowObject)
        {
            Deactivate();
            return;
        }

        const XMFLOAT3 dir = GetForwardFromObject(m_directionSource ? m_directionSource : owner);

        if (auto* tr = owner->GetComponent<CTransformComponent>())
        {
            tr->SetLookDirection(dir);
        }

        XMFLOAT3 pos = m_bowObject->GetPosition();
        pos.x -= dir.x * m_pullBackDistance;
        pos.y -= dir.y * m_pullBackDistance;
        pos.z -= dir.z * m_pullBackDistance;

        owner->SetPosition(pos);
        return;
    }

    if (m_state == EState::Flying)
    {
        if (m_lifeRemaining > 0.0f)
        {
            m_lifeRemaining -= dt;
            if (m_lifeRemaining <= 0.0f)
            {
                Deactivate();
                return;
            }
        }

		m_velocity.y += kArrowGravityY * dt;

		const XMFLOAT3 pos = owner->GetPosition();
		const XMFLOAT3 next =
		{
			pos.x + m_velocity.x * dt,
			pos.y + m_velocity.y * dt,
			pos.z + m_velocity.z * dt
		};

		owner->SetPosition(next);

		if ( auto* tr = owner->GetComponent<CTransformComponent>() )
		{
			tr->SetLookDirection(NormalizeSafe(m_velocity));
		}
    }
}