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
	m_activeBoneWeaponConfigIndex = -1;

	if ( auto* collider = GetOwner() ? GetOwner()->GetComponent<CColliderComponent>() : nullptr )
	{
		if ( m_useOwnerBoneWeaponCapsules )
		{
			collider->SetWeaponCapsulesActive(false);
			collider->ClearWeaponBoneCapsuleRoots();
		}
		else
		{
			collider->SetCollisionEnabled(false);
		}
	}
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

void CMonsterWeaponHitboxComponent::AddBoneWeaponConfig(
	const std::string& clipName,
	float startNormalized,
	float endNormalized,
	const std::vector<std::string>& rootBoneNames)
{
	BoneWeaponConfig cfg{};
	cfg.clipName = clipName;
	cfg.startNormalized = Clamp01(startNormalized);
	cfg.endNormalized = Clamp01(endNormalized);
	cfg.rootBoneNames = rootBoneNames;

	if ( cfg.endNormalized < cfg.startNormalized )
	{
		const float t = cfg.startNormalized;
		cfg.startNormalized = cfg.endNormalized;
		cfg.endNormalized = t;
	}

	m_boneWeaponConfigs.push_back(std::move(cfg));
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

int CMonsterWeaponHitboxComponent::FindActiveBoneWeaponConfigIndex() const
{
	if ( !m_pAttacker )
		return -1;

	if ( m_boneWeaponConfigs.empty() )
		return -1;

	auto* animComp = m_pAttacker->GetComponent<CAnimatorComponent>();
	if ( !animComp )
		return -1;

	auto* monsterCtrl = animComp->GetMonsterController();
	auto* animator = animComp->GetAnimator();

	if ( !monsterCtrl || !animator )
		return -1;

	if ( !monsterCtrl->IsAttackPrimaryPhase() )
		return -1;

	const std::string& currentClip = animator->GetCurrentClipName();

	float normalized = 0.0f;
	const float duration = animator->GetCurrentClipDuration();
	if ( duration > 1e-6f )
	{
		const float curTime = animator->GetCurrentTime();
		normalized = Clamp01(curTime / duration);
	}

	for ( int i = 0; i < ( int ) m_boneWeaponConfigs.size(); ++i )
	{
		const BoneWeaponConfig& cfg = m_boneWeaponConfigs[i];

		if ( cfg.clipName != currentClip )
			continue;

		if ( normalized < cfg.startNormalized )
			continue;

		if ( normalized > cfg.endNormalized )
			continue;

		return i;
	}

	return -1;
}

void CMonsterWeaponHitboxComponent::OnUpdate(float /*dt*/)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	auto* collider = owner->GetComponent<CColliderComponent>();
	if ( !collider )
		return;

	if ( m_useOwnerBoneWeaponCapsules )
	{
		const int activeConfigIndex = FindActiveBoneWeaponConfigIndex();
		const bool active = ( activeConfigIndex >= 0 );

		if ( activeConfigIndex != m_activeBoneWeaponConfigIndex )
		{
			ClearHitTargets();
			m_activeBoneWeaponConfigIndex = activeConfigIndex;

			if ( activeConfigIndex >= 0 )
				collider->SetWeaponBoneCapsuleRoots(m_boneWeaponConfigs[activeConfigIndex].rootBoneNames);
			else
				collider->ClearWeaponBoneCapsuleRoots();
		}

		collider->SetWeaponCapsulesActive(active);
		m_bPrevHitboxActive = active;
		return;
	}

	const bool active = IsAttackWindowOpen();

	if ( active != m_bPrevHitboxActive )
		ClearHitTargets();

	collider->SetCollisionEnabled(active);
	m_bPrevHitboxActive = active;
}