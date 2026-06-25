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

	void SetHitReactionPolicy(
		bool onlyWhenNoAction,
		float animSuperArmorSec)
	{
		m_bHitReactionOnlyWhenNoAction = onlyWhenNoAction;
		m_hitReactionAnimSuperArmorDurationSec =
			( animSuperArmorSec > 0.0f ) ? animSuperArmorSec : 0.0f;
	}

	void SetLocomotionState(EMonsterAnimState state) { m_locomotionState = state; }
	EMonsterAnimState GetLocomotionState() const { return m_locomotionState; }

	void ResetRuntimeState(EMonsterAnimState locomotionState = EMonsterAnimState::Idle);
	void PlayDeathFromStart();
	void PlayDeathFinalPose();
	void RequestCommand(EMonsterAnimCommand cmd);
	void Update(float dt);

	bool IsBusy() const { return m_actionPhase != EActionPhase::None; }
	bool HasPendingCommand() const { return m_pendingCommand != EMonsterAnimCommand::None; }
	bool BlocksAIControl() const { return IsBusy() || HasPendingCommand(); }

	bool IsAttackPrimaryPhase() const { return m_actionPhase == EActionPhase::Attack; }
	bool IsAttackChainPhase() const { return m_actionPhase == EActionPhase::AttackChainNext; }
	bool IsSpellPhase() const { return m_actionPhase == EActionPhase::Spell; }
	bool IsCallPhase() const { return m_actionPhase == EActionPhase::Call; }
	bool IsAppearPhase() const { return m_actionPhase == EActionPhase::Appear; }

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

	bool CanAcceptHitReactionCommand() const;

	void StartLocomotionIfNeeded(CAnimator* anim);
	bool StartAction(CAnimator* anim, const std::string& clipName, EActionPhase phase, float blendTimeSec, bool loop);

private:
	CGameObject* m_pOwner = nullptr;

	MonsterAnimProfile m_profile{};

	EMonsterAnimState m_locomotionState = EMonsterAnimState::Idle;
	EMonsterAnimCommand m_pendingCommand = EMonsterAnimCommand::None;

	EActionPhase m_actionPhase = EActionPhase::None;

	// 보스 전용으로 사용.
	// true면 현재 액션 중에는 Hit 애니메이션을 받지 않는다.
	// 대미지 무적이 아니라 애니메이션 캔슬 방지용 슈퍼아머다.
	bool m_bHitReactionOnlyWhenNoAction = false;
	float m_hitReactionAnimSuperArmorDurationSec = 0.0f;
	float m_hitReactionAnimSuperArmorRemainingSec = 0.0f;
};