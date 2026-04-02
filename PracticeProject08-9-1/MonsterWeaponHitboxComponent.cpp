//-----------------------------------------------------------------------------
// File: MonsterWeaponHitboxComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "MonsterWeaponHitboxComponent.h"

#include "Object.h"
#include "ColliderComponent.h"
#include "AnimatorComponent.h"
#include "MonsterAnimController.h"
#include "Animator.h"

namespace
{
	static float Clamp01(float v)
	{
		if ( v < 0.0f ) return 0.0f;
		if ( v > 1.0f ) return 1.0f;
		return v;
	}
}

CMonsterWeaponHitboxComponent::CMonsterWeaponHitboxComponent(CGameObject* owner)
	: CComponentT<CMonsterWeaponHitboxComponent>(owner)
{
}

void CMonsterWeaponHitboxComponent::OnCreate(ID3D12Device* /*dev*/, ID3D12GraphicsCommandList* /*cmd*/)
{
	m_bPrevHitboxActive = false;

	if ( auto* collider = GetOwner() ? GetOwner()->GetComponent<CColliderComponent>() : nullptr )
		collider->SetCollisionEnabled(false);
}

void CMonsterWeaponHitboxComponent::SetActiveWindow(float startNormalized, float endNormalized)
{
	m_activeStartNormalized = Clamp01(startNormalized);
	m_activeEndNormalized = Clamp01(endNormalized);

	if ( m_activeEndNormalized < m_activeStartNormalized )
	{
		const float t = m_activeStartNormalized;
		m_activeStartNormalized = m_activeEndNormalized;
		m_activeEndNormalized = t;
	}
}

bool CMonsterWeaponHitboxComponent::CanHitTarget(CGameObject* target) const
{
	if ( !target ) return false;
	return ( m_hitTargets.find(target) == m_hitTargets.end() );
}

void CMonsterWeaponHitboxComponent::MarkHitTarget(CGameObject* target)
{
	if ( !target ) return;
	m_hitTargets.insert(target);
}

bool CMonsterWeaponHitboxComponent::IsAttackWindowOpen() const
{
	if ( !m_pAttacker )
		return false;

	auto* animComp = m_pAttacker->GetComponent<CAnimatorComponent>();
	if ( !animComp )
		return false;

	auto* monsterCtrl = animComp->GetMonsterController();
	auto* animator = animComp->GetAnimator();

	if ( !monsterCtrl || !animator )
		return false;

	if ( !monsterCtrl->IsAttackPrimaryPhase() )
		return false;

	if ( !m_attackClipName.empty() )
	{
		if ( animator->GetCurrentClipName() != m_attackClipName )
			return false;
	}

	const float duration = animator->GetCurrentClipDuration();
	if ( duration <= 1e-6f )
		return true;

	const float curTime = animator->GetCurrentTime();
	const float normalized = Clamp01(curTime / duration);

	return ( normalized >= m_activeStartNormalized &&
			 normalized <= m_activeEndNormalized );
}

void CMonsterWeaponHitboxComponent::OnUpdate(float /*dt*/)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	auto* collider = owner->GetComponent<CColliderComponent>();
	if ( !collider )
		return;

	const bool active = IsAttackWindowOpen();

	if ( active != m_bPrevHitboxActive )
		ClearHitTargets();

	collider->SetCollisionEnabled(active);
	m_bPrevHitboxActive = active;
}