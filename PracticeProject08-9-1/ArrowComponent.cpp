//-----------------------------------------------------------------------------
// File: ArrowComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "ArrowComponent.h"
#include "Object.h" // CGameObject

CArrowComponent::CArrowComponent(CGameObject* owner)
    : CComponentT<CArrowComponent>(owner)
{
}

void CArrowComponent::Activate(const XMFLOAT3& position, const XMFLOAT3& velocity, float lifeSec)
{
    CGameObject* owner = GetOwner();
    if (!owner) return;

    owner->SetPosition(position);

    m_velocity = velocity;
    m_lifeRemaining = (lifeSec > 0.0f) ? lifeSec : 0.0f;
    m_active = true;
}

void CArrowComponent::Deactivate()
{
    CGameObject* owner = GetOwner();
    if (owner)
    {
        // 화면/월드에서 사실상 숨김(바닥 아래로 이동)
        owner->SetPosition(0.0f, -10000.0f, 0.0f);
    }

    m_active = false;
    m_lifeRemaining = 0.0f;
    m_velocity = { 0.0f, 0.0f, 0.0f };
}

void CArrowComponent::OnUpdate(float dt)
{
    if (!m_active) return;

    CGameObject* owner = GetOwner();
    if (!owner) return;

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