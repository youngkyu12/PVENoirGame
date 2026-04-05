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
	// 한 번 휘두를 때 같은 대상 중복 타격 방지용 캐시를 이미 쓰고 있으면
	// 그 캐시 clear 함수 이름만 맞춰서 연결하면 된다.
	void ClearHitTargets() { m_hitTargets.clear(); }
	bool CanHitTarget(CGameObject* target) const;
	void MarkHitTarget(CGameObject* target);

private:
	void DisableAllWeaponColliders();

private:
	bool m_bPrevHitboxActive = false;
	std::unordered_set<CGameObject*> m_hitTargets;
};