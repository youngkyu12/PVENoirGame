//-----------------------------------------------------------------------------
// File: WeaponHitboxComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "WeaponHitboxComponent.h"

#include "Object.h"
#include "AnimatorComponent.h"
#include "AnimController.h"
#include "PlayerEquipmentComponent.h"
#include "ColliderComponent.h"

CWeaponHitboxComponent::CWeaponHitboxComponent(CGameObject* owner)
	: CComponentT<CWeaponHitboxComponent>(owner)
{
}

void CWeaponHitboxComponent::OnCreate(ID3D12Device* /*dev*/, ID3D12GraphicsCommandList* /*cmd*/)
{
	m_bPrevHitboxActive = false;
	DisableAllWeaponColliders();
}

void CWeaponHitboxComponent::OnUpdate(float /*dt*/)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	auto* equip = owner->GetComponent<CPlayerEquipmentComponent>();
	if ( !equip )
	{
		DisableAllWeaponColliders();
		m_bPrevHitboxActive = false;
		return;
	}

	CAnimController* ctrl = nullptr;

	if ( auto* animComp = owner->GetComponent<CAnimatorComponent>() )
		ctrl = animComp->GetController();

	if ( !ctrl )
		ctrl = owner->GetAnimController();

	const bool shouldEnableMeleeHitbox =
		( ctrl != nullptr ) && ctrl->IsMeleeAttackHitboxActive();

	// 기본은 전부 끔
	DisableAllWeaponColliders();

	// 공격 시작/종료 경계에서 히트 캐시 초기화
	if ( shouldEnableMeleeHitbox != m_bPrevHitboxActive )
		ClearHitTargets();

	if ( shouldEnableMeleeHitbox )
	{
		const EWeaponType equipped = equip->GetEquippedWeapon();

		if ( equipped == EWeaponType::Sword || equipped == EWeaponType::Axe )
		{
			CGameObject* weaponObj = equip->GetEquippedWeaponObject();
			if ( weaponObj )
			{
				if ( auto* collider = weaponObj->GetComponent<CColliderComponent>() )
					collider->SetCollisionEnabled(true);
			}
		}
	}

	m_bPrevHitboxActive = shouldEnableMeleeHitbox;
}

void CWeaponHitboxComponent::DisableAllWeaponColliders()
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	auto* equip = owner->GetComponent<CPlayerEquipmentComponent>();
	if ( !equip )
		return;

	auto DisableOne = [ ] (CGameObject* weaponObj)
		{
			if ( !weaponObj ) return;
			if ( auto* collider = weaponObj->GetComponent<CColliderComponent>() )
				collider->SetCollisionEnabled(false);
		};

	DisableOne(equip->GetWeaponObject(EWeaponType::Sword));
	DisableOne(equip->GetWeaponObject(EWeaponType::Axe));
	DisableOne(equip->GetWeaponObject(EWeaponType::Bow));
	DisableOne(equip->GetWeaponObject(EWeaponType::Gun));
}