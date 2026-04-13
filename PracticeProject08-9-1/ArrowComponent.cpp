#include "stdafx.h"
#include "ArrowComponent.h"
#include "Object.h"
#include "ColliderComponent.h"

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

void CArrowComponent::Activate(const XMFLOAT3& position, const XMFLOAT3& velocity, float lifeSec)
{
    CGameObject* owner = GetOwner();
    if (!owner) return;

    owner->SetPosition(position);

    m_bowObject = nullptr;
    m_directionSource = nullptr;
    m_pullBackDistance = 0.0f;

	m_velocity = velocity;
	m_lifeRemaining = ( lifeSec > 0.0f ) ? lifeSec : 0.0f;
	m_enableCollisionOnLaunch = true;
	m_state = EState::Flying;

	if ( auto* collider = owner->GetComponent<CColliderComponent>() )
	{
		collider->SetCollisionEnabled(true);
	}	

	
}

void CArrowComponent::Prepare(
	CGameObject* bowObject,
	CGameObject* directionSource,
	float pullBackDistance,
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
	m_state = EState::Prepared;

	if ( auto* collider = owner->GetComponent<CColliderComponent>() )
	{
		collider->SetCollisionEnabled(false);
	}

	OnUpdate(0.0f); // 즉시 스냅
}

void CArrowComponent::Launch(const XMFLOAT3& velocity, float lifeSec, bool enableCollision) 
{
    if (m_state != EState::Prepared)
        return;

    m_bowObject = nullptr;
    m_directionSource = nullptr;
    m_pullBackDistance = 0.0f;

	m_velocity = velocity;
	m_lifeRemaining = ( lifeSec > 0.0f ) ? lifeSec : 0.0f;
	m_state = EState::Flying;

	CGameObject* owner = GetOwner();
	if ( owner )
	{
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

        const XMFLOAT3 pos = owner->GetPosition();
        const XMFLOAT3 next =
        {
            pos.x + m_velocity.x * dt,
            pos.y + m_velocity.y * dt,
            pos.z + m_velocity.z * dt
        };

        owner->SetPosition(next);
    }
}