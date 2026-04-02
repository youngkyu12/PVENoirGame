//-----------------------------------------------------------------------------
// File: WeaponHitboxComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "WeaponHitboxComponent.h"

#include "Object.h"
#include "ColliderComponent.h"
#include "AnimController.h"

CWeaponHitboxComponent::CWeaponHitboxComponent(CGameObject* owner)
	: CComponentT<CWeaponHitboxComponent>(owner)
{
}

void CWeaponHitboxComponent::OnCreate(ID3D12Device* /*dev*/, ID3D12GraphicsCommandList* /*cmd*/)
{
	m_pEquipment = GetOwner()->GetComponent<CPlayerEquipmentComponent>();
	DisableAllMeleeWeaponColliders();
}

void CWeaponHitboxComponent::OnUpdate(float /*dt*/)
{
	if ( !m_pEquipment )
		return;

	const EWeaponType equipped = m_pEquipment->GetEquippedWeapon();
	const bool active = IsMeleeAttackActive();

	SetWeaponColliderEnabled(
		EWeaponType::Sword,
		active && ( equipped == EWeaponType::Sword )
	);

	SetWeaponColliderEnabled(
		EWeaponType::Axe,
		active && ( equipped == EWeaponType::Axe )
	);

	// 원거리 무기는 근접 히트박스 사용 안 함
	SetWeaponColliderEnabled(EWeaponType::Bow, false);
	SetWeaponColliderEnabled(EWeaponType::Gun, false);
}

bool CWeaponHitboxComponent::IsMeleeAttackActive() const
{
	if ( !m_pEquipment )
		return false;

	const EWeaponType equipped = m_pEquipment->GetEquippedWeapon();
	if ( equipped != EWeaponType::Sword && equipped != EWeaponType::Axe )
		return false;

	auto* ctrl = GetOwner()->GetAnimController();
	if ( !ctrl )
		return false;

	return ( ctrl->GetAnimState() == EAnimState::Attack );
}

void CWeaponHitboxComponent::DisableAllMeleeWeaponColliders()
{
	SetWeaponColliderEnabled(EWeaponType::Sword, false);
	SetWeaponColliderEnabled(EWeaponType::Axe, false);
}

void CWeaponHitboxComponent::SetWeaponColliderEnabled(EWeaponType type, bool enabled)
{
	if ( !m_pEquipment )
		return;

	CGameObject* weaponObj = m_pEquipment->GetWeaponObject(type);
	if ( !weaponObj )
		return;

	auto* collider = weaponObj->GetComponent<CColliderComponent>();
	if ( !collider )
		return;

	collider->SetCollisionEnabled(enabled);
}