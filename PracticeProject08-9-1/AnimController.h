//------------------------------------------------------- ----------------------
// File: AnimatorController.h
//-----------------------------------------------------------------------------


#pragma once
#include "stdafx.h"
#include "GlobalEnum.h"

class CGameObject;
class CAnimator;

class CAnimController
{
public:
    CAnimController() = default;
    explicit CAnimController(CGameObject* owner) { Bind(owner); }

public:
    void Bind(CGameObject* owner) { m_pOwner = owner; }

    void SetSpeed(float s) { m_speed = s; }

    void SetIdleClip(const char* name) { m_idleClip = name; }
    void SetMoveClip(const char* name) { m_moveClip = name; }
    void SetHitClip(const char* name) { m_hitClip = name; }
    void SetAttackClip(const char* name) { m_attackClip = name; }

    void SetIdleClip(const std::string& name) { m_idleClip = name; }
    void SetMoveClip(const std::string& name) { m_moveClip = name; }
    void SetHitClip(const std::string& name) { m_hitClip = name; }
    void SetAttackClip(const std::string& name) { m_attackClip = name; }

    void SetMoveDirection(uint32_t dirBits) { m_moveDirBits = dirBits; }
    uint32_t GetMoveDirection() const { return m_moveDirBits; }

    void SetRunRequested(bool run) { m_bRunRequested = run; }
    bool IsRunRequested() const { return m_bRunRequested; }

    void EnablePlayerClipSet(bool enable = true) { m_usePlayerClipSet = enable; }
    bool IsUsingPlayerClipSet() const { return m_usePlayerClipSet; }

    bool IsActionLocked() const;

    void Update(float dt);

    bool RequestAttack();
    bool RequestRoll(uint32_t dirBits);
    bool IsBowLoadPhase() const { return m_actionPhase == EActionPhase::AttackBowLoad; }
    bool IsBowReleasePhase() const { return m_actionPhase == EActionPhase::AttackBowRelease; }

    void RequestHit();

private:
    const char* ClipFor(EAnimState s) const
    {
        return (s == EAnimState::Move) ? m_moveClip.c_str() : m_idleClip.c_str();
    }

public:
    void SetAnimState(EAnimState s)
    {
        m_state = s;
    }

private:
    enum class EActionPhase : uint8_t
    {
        None = 0,
        AttackGeneric,
        AttackBowLoad,
        AttackBowRelease,
        Hit,
        Roll
    };

private:
    std::string ResolveIdleClip() const;
    std::string ResolveMoveClip() const;
    std::string ResolveHitClip() const;
    std::string ResolveAttackStartClip(EActionPhase& outPhase) const;
    std::string ResolveRollClip(uint32_t dirBits, float& outVisualYawDeg) const;
    std::string ResolveLocomotionClip(EAnimState state) const;

private:
    EAnimState animPrevState = EAnimState::Idle;

private:
    CGameObject* m_pOwner = nullptr;

    float m_speed = 0.0f;
    float m_moveEps = 0.05f;

    EAnimState m_state = EAnimState::Idle;

    std::string m_idleClip = "Idle";
    std::string m_moveClip = "Walk";
    std::string m_hitClip = "Hit";
    std::string m_attackClip = "Attack";

    bool m_attackQueued = false;
    bool m_hitQueued = false;

    bool m_rollQueued = false;
    std::string m_rollQueuedClipName;
    float m_rollQueuedVisualYawDeg = 0.0f;

    uint32_t m_moveDirBits = 0;
    bool m_bRunRequested = false;
    bool m_usePlayerClipSet = false;

    EActionPhase m_actionPhase = EActionPhase::None;

	float m_startTime = 0.0f;
};
