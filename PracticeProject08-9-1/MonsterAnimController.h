//------------------------------------------------------- ----------------------
// File: MonsterAnimController.h
//-----------------------------------------------------------------------------

#pragma once
#include "stdafx.h"
#include "MonsterAnimTypes.h"

class CGameObject;
class CAnimator;

class CMonsterAnimController
{
public:
	CMonsterAnimController() = default;
	explicit CMonsterAnimController(CGameObject* owner) { Bind(owner); }

public:
	void Bind(CGameObject* owner) { m_pOwner = owner; }

	void SetProfile(const MonsterAnimProfile& profile) { m_profile = profile; }

	void SetLocomotionState(EMonsterAnimState state) { m_locomotionState = state; }
	EMonsterAnimState GetLocomotionState() const { return m_locomotionState; }

	void RequestCommand(EMonsterAnimCommand cmd);
	void Update(float dt);

	bool IsBusy() const { return m_actionPhase != EActionPhase::None; }
	bool HasPendingCommand() const { return m_pendingCommand != EMonsterAnimCommand::None; }
	bool BlocksAIControl() const { return IsBusy() || HasPendingCommand(); }

	bool IsAttackPrimaryPhase() const { return m_actionPhase == EActionPhase::Attack; }
	bool IsAttackChainPhase() const { return m_actionPhase == EActionPhase::AttackChainNext; }

private:
	enum class EActionPhase : uint8_t
	{
		None = 0,
		Attack,
		AttackChainNext,
		Hit,
		Death,
		Appear,
		Call,
		Spell
	};

private:
	CAnimator* ResolveAnimator() const;
	std::string ResolveLocomotionClip() const;

	void StartLocomotionIfNeeded(CAnimator* anim);
	bool StartAction(CAnimator* anim, const std::string& clipName, EActionPhase phase, float blendTimeSec, bool loop);

private:
	CGameObject* m_pOwner = nullptr;

	MonsterAnimProfile m_profile{};

	EMonsterAnimState m_locomotionState = EMonsterAnimState::Idle;
	EMonsterAnimCommand m_pendingCommand = EMonsterAnimCommand::None;

	EActionPhase m_actionPhase = EActionPhase::None;
};