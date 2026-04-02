//-----------------------------------------------------------------------------
// File: WeaponHitboxComponent.h
//-----------------------------------------------------------------------------
#pragma once

#include "Component.h"
#include "PlayerEquipmentComponent.h"

class CPlayerEquipmentComponent;
class CColliderComponent;

class CWeaponHitboxComponent final : public CComponentT<CWeaponHitboxComponent>
{
public:
	explicit CWeaponHitboxComponent(CGameObject* owner);

	void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;
	void OnUpdate(float dt) override;

private:
	void SetWeaponColliderEnabled(EWeaponType type, bool enabled);
	void DisableAllMeleeWeaponColliders();
	bool IsMeleeAttackActive() const;

private:
	CPlayerEquipmentComponent* m_pEquipment = nullptr;
};