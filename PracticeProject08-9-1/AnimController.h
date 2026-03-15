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
    void SetIdleClip(const std::string& name) { m_idleClip = name; }
    void SetMoveClip(const std::string& name) { m_moveClip = name; }

    void Update(float dt);

    void SetAttackClip(const char* name) { m_attackClip = name; }
    void SetAttackClip(const std::string& name) { m_attackClip = name; }

    void RequestAttack();


private:
    const char* ClipFor(EAnimState s) const
    {
        return (s == EAnimState::Move) ? m_moveClip.c_str() : m_idleClip.c_str();
    }

public:
    // DEMO: status °­Á¦ SEt
    void SetAnimState(EAnimState s)
    {
        m_state = s;
        m_attackQueued = false;
	}

private:
    EAnimState animPrevState;

private:
    CGameObject* m_pOwner = nullptr;
    float m_speed = 0.0f;
    float m_moveEps = 0.05f;
    EAnimState m_state = EAnimState::Idle;

    std::string m_idleClip = "Idle";
    std::string m_moveClip = "Walk";
    std::string m_attackClip = "Attack";
    bool m_attackQueued = false;

	float m_startTime = 0.0f;
};
