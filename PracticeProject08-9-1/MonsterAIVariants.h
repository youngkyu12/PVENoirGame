//-----------------------------------------------------------------------------
// File: MonsterAIVariants.h
//-----------------------------------------------------------------------------

#pragma once

#include "MonsterAIComponent.h"

//-----------------------------------------------------------------------------
// Ghoul
//-----------------------------------------------------------------------------
class CGhoulAIComponent final : public CMonsterAIComponent
{
public:
	explicit CGhoulAIComponent(CGameObject* owner);
	~CGhoulAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CGhoulAIComponent>();
	}

protected:
	bool TryPerformAttack() override;
};

//-----------------------------------------------------------------------------
// SwordMan
//-----------------------------------------------------------------------------
class CSwordManAIComponent final : public CMonsterAIComponent
{
public:
	explicit CSwordManAIComponent(CGameObject* owner);
	~CSwordManAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CSwordManAIComponent>();
	}

protected:
	bool TryPerformAttack() override;
};

//-----------------------------------------------------------------------------
// BowMan
//-----------------------------------------------------------------------------
class CBowManAIComponent final : public CMonsterAIComponent
{
public:
	explicit CBowManAIComponent(CGameObject* owner);
	~CBowManAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CBowManAIComponent>();
	}

protected:
	bool CanStartAttackAgainstTarget() const override;
	bool TryPerformAttack() override;
};

//-----------------------------------------------------------------------------
// Mutant
//-----------------------------------------------------------------------------
class CMutantAIComponent final : public CMonsterAIComponent
{
public:
	explicit CMutantAIComponent(CGameObject* owner);
	~CMutantAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CMutantAIComponent>();
	}

protected:
	bool TryPerformAttack() override;
};

//-----------------------------------------------------------------------------
// Boss
//-----------------------------------------------------------------------------
class CBossAIComponent final : public CMonsterAIComponent
{
public:
	explicit CBossAIComponent(CGameObject* owner);
	~CBossAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CBossAIComponent>();
	}

protected:
	bool AcquireTarget() override;
	void UpdateBehavior(float dt) override;
	bool TryPerformAttack() override;

	bool CanMoveNow() const override;
	bool CanThinkNow() const override;
	bool CanRotateNow() const override;

	EMonsterAnimState GetChaseLocomotionState() const override;
	EMonsterAnimState GetWalkLocomotionState() const override;

private:
	enum class EBossAttackIntent : uint8_t
	{
		Melee = 0,
		Spell
	};

private:
	void UpdateBossCooldowns(float dt);
	bool IsPlayerInsideBossBattleZone(CGameObject* player) const;
	bool CanStartBossAction() const;

	bool TryPerformBossCommand(EMonsterAnimCommand command);
	bool TryPerformMeleeAttack();
	bool TryPerformSpellAttack();

	void ConsumeBossMeleeCooldown();
	void ConsumeBossSpellCooldown();

	bool IsBossMeleeActionPlaying() const;
	bool IsBossSpellActionPlaying() const;

	bool SmoothFaceTowardsTarget(
		CGameObject* target,
		float dt,
		float turnSpeedDegreesPerSec
	);

	bool UpdateBossPostMeleeTurn(float dt);

private:
	EBossAttackIntent m_pendingAttackIntent = EBossAttackIntent::Melee;

	float m_bossMeleeRange = 7.0f;
	float m_bossPreferredSpellRange = 12.0f;

	float m_bossGlobalActionCooldown = 0.8f;
	float m_bossMeleeCooldown = 2.0f;
	float m_bossSpellCooldown = 3.5f;

	float m_bossGlobalActionCooldownRemaining = 0.0f;
	float m_bossMeleeCooldownRemaining = 0.0f;
	float m_bossSpellCooldownRemaining = 0.0f;

	bool m_bBossWasMeleeActionPlaying = false;

	float m_bossPostMeleeTurnDuration = 0.25f;
	float m_bossPostMeleeTurnRemaining = 0.0f;
	float m_bossPostMeleeTurnSpeedDegrees = 900.0f;

	float m_bossSpellTurnSpeedDegrees = 720.0f;
};