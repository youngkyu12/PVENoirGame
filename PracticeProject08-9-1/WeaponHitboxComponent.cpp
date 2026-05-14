//-----------------------------------------------------------------------------
// File: WeaponHitboxComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "WeaponHitboxComponent.h"

#include "Object.h"
#include "ColliderComponent.h"

CWeaponHitboxComponent::CWeaponHitboxComponent(CGameObject* owner)
	: CComponentT<CWeaponHitboxComponent>(owner)
{
}

void CWeaponHitboxComponent::OnCreate(
	ID3D12Device* /*dev*/,
	ID3D12GraphicsCommandList* /*cmd*/)
{
	m_bHitboxActive = false;
	m_hitTargets.clear();

	if ( CGameObject* owner = GetOwner() )
	{
		if ( auto* collider = owner->GetComponent<CColliderComponent>() )
			collider->SetCollisionEnabled(false);
	}
}

void CWeaponHitboxComponent::OnUpdate(float /*dt*/)
{
	// 무기 hitbox 활성/비활성은 GameScene::UpdateSwordTrails()에서 제어한다.
}

void CWeaponHitboxComponent::SetHitboxActive(bool active)
{
	if ( m_bHitboxActive == active )
	{
		if ( CGameObject* owner = GetOwner() )
		{
			if ( auto* collider = owner->GetComponent<CColliderComponent>() )
				collider->SetCollisionEnabled(active);
		}

		return;
	}

	m_bHitboxActive = active;

	if ( active )
		ClearHitTargets();

	if ( CGameObject* owner = GetOwner() )
	{
		if ( auto* collider = owner->GetComponent<CColliderComponent>() )
		{
			collider->SetCollisionEnabled(active);

			if ( active )
				collider->UpdateWorldBounds();
		}
	}
}

bool CWeaponHitboxComponent::CanHitTarget(CGameObject* target) const
{
	if ( !m_bHitboxActive )
		return false;

	if ( !target )
		return false;

	return m_hitTargets.find(target) == m_hitTargets.end();
}

void CWeaponHitboxComponent::MarkHitTarget(CGameObject* target)
{
	if ( !target )
		return;

	m_hitTargets.insert(target);
}