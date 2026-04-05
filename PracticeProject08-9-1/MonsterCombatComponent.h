//-----------------------------------------------------------------------------
// File: MonsterCombatComponent.h
//-----------------------------------------------------------------------------
#pragma once

#include "Component.h"

class CGameObject;

class CMonsterCombatComponent final : public CComponentT<CMonsterCombatComponent>
{
public:
	explicit CMonsterCombatComponent(CGameObject* owner);

	void OnUpdate(float dt) override;

public:
	// 플레이어 무기에 맞았을 때 호출
	// forceDeath=true 면 Hit 대신 즉시 Death 테스트
	void OnHitByPlayerWeapon(CGameObject* weaponObject, bool forceDeath = false);

	// 외부 테스트용
	void ForceDeath();

public:
	bool CanTakeHit() const;
	bool IsDead() const { return m_bDead; }

	void SetHitCooldown(float sec) { m_hitCooldownSec = ( sec < 0.0f ) ? 0.0f : sec; }
	float GetHitCooldown() const { return m_hitCooldownSec; }

private:
	void RequestHitAnimation();
	void RequestDeathAnimation();

private:
	float m_hitCooldownSec = 0.25f;
	float m_hitCooldownRemaining = 0.0f;
	bool  m_bDead = false;
};