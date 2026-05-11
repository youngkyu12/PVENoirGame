//-----------------------------------------------------------------------------
// File: WeaponHitboxComponent.h
//-----------------------------------------------------------------------------
#pragma once

#include "Component.h"
#include <unordered_set>

class CGameObject;

class CWeaponHitboxComponent final : public CComponentT<CWeaponHitboxComponent>
{
public:
	explicit CWeaponHitboxComponent(CGameObject* owner);

	void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;
	void OnUpdate(float dt) override;

public:
	void SetHitboxActive(bool active);
	bool IsHitboxActive() const { return m_bHitboxActive; }

	void ClearHitTargets() { m_hitTargets.clear(); }
	bool CanHitTarget(CGameObject* target) const;
	void MarkHitTarget(CGameObject* target);

private:
	bool m_bHitboxActive = false;
	std::unordered_set<CGameObject*> m_hitTargets;
};