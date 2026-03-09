//===========================================================================
// ServerObject.cpp
//============================================================================

#include "pch.h"
#include "ServerObject.h"
#include "PlayerLogic.h"

// ============================================================================
// 생성자 / 소멸자
// ============================================================================
CServerObject::CServerObject()
{
    m_components.reserve(4);
    m_pTransform = AddComponent<CCommonTransformComponent>();
}

CServerObject::~CServerObject()
{
    DestroyComponents();
}

// ============================================================================
// Transform
// ============================================================================
void CServerObject::SetPosition(float x, float y, float z)
{
    if (m_pTransform)
        m_pTransform->SetPosition(x, y, z);
}

void CServerObject::SetPosition(const GameMath::Vec3& pos)
{
    if (m_pTransform)
        m_pTransform->SetPosition(pos);
}

GameMath::Vec3 CServerObject::GetPosition() const
{
    return m_pTransform ? m_pTransform->GetPosition() : GameMath::Vec3::Zero();
}

void CServerObject::SetYaw(float degrees)
{
    if (m_pTransform)
        m_pTransform->SetYawDegrees(degrees);
}

float CServerObject::GetYaw() const
{
    return m_pTransform ? m_pTransform->GetYawDegrees() : 0.f;
}

void CServerObject::Rotate(float pitchDeg, float yawDeg, float rollDeg)
{
    if (m_pTransform)
    {
        m_pTransform->RotateYaw(yawDeg);
        // pitch, roll은 현재 무시 (필요시 추가)
    }
}

GameMath::Vec3 CServerObject::GetLook() const
{
    return m_pTransform ? m_pTransform->GetLook() : GameMath::Vec3::Forward();
}

GameMath::Vec3 CServerObject::GetRight() const
{
    return m_pTransform ? m_pTransform->GetRight() : GameMath::Vec3::Right();
}

GameMath::Vec3 CServerObject::GetUp() const
{
    return m_pTransform ? m_pTransform->GetUp() : GameMath::Vec3::Up();
}

// ============================================================================
// Movement
// ============================================================================
void CServerObject::MoveForward(float distance)
{
    Move(GetLook() * distance);
}

void CServerObject::MoveStrafe(float distance)
{
    Move(GetRight() * distance);
}

void CServerObject::MoveUp(float distance)
{
    if (m_pTransform)
        m_pTransform->Translate(0.f, distance, 0.f);
}

void CServerObject::Move(const GameMath::Vec3& shift)
{
    if (m_pTransform)
        m_pTransform->Translate(shift);
}

// ============================================================================
// Physics
// ============================================================================
void CServerObject::ApplyPhysics(float dt)
{
    if (!m_pTransform) return;

    GameMath::Vec3 pos = m_pTransform->GetPosition();

    PlayerLogic::ApplyPhysics(
        pos,
        m_velocity,
        m_gravity,
        m_friction,
        m_maxVelXZ,
        m_maxVelY,
        dt
    );

    m_pTransform->SetPosition(pos);
}

// ============================================================================
// Update
// ============================================================================
void CServerObject::Update(float dt)
{
    ApplyPhysics(dt);
    UpdateComponents(dt);
}

void CServerObject::Animate(float dt)
{
    m_animTick += dt;
}

void CServerObject::SetAnimState(Protocol::AnimationType state)
{
	m_animState = state;
}

// ============================================================================
// Components
// ============================================================================
void CServerObject::CreateComponents()
{
    if (m_bComponentsCreated) return;
    m_bComponentsCreated = true;

    for (auto& c : m_components)
    {
        if (c && c->IsEnabled())
            c->OnCreate();
    }
}

void CServerObject::DestroyComponents()
{
    // 역순 파괴
    for (int i = static_cast<int>(m_components.size()) - 1; i >= 0; --i)
    {
        if (m_components[i])
            m_components[i]->OnDestroy();
    }

    m_components.clear();
    m_pTransform = nullptr;
    m_bComponentsCreated = false;
}

void CServerObject::UpdateComponents(float dt)
{
    for (auto& c : m_components)
    {
        if (c && c->IsEnabled())
            c->OnUpdate(dt);
    }

    for (auto& c : m_components)
    {
        if (c && c->IsEnabled())
            c->OnLateUpdate(dt);
    }
}

// ============================================================================
// Network State
// ============================================================================
NetworkPlayerState CServerObject::ToNetworkState() const
{
    return NetworkPlayerState::FromGameMath(
        m_objectId,
        GetPosition(),
        GetYaw(),
        m_animState,
        m_animTick
    );
}

void CServerObject::ApplyNetworkState(const NetworkPlayerState& state)
{
    m_objectId = state.playerId;
    SetPosition(state.transform.ToVec3());
    SetYaw(state.transform.yaw);
    //m_animState = state.anim.state;
    /*m_animTick = state.anim.time;*/
}
