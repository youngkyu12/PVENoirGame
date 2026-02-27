//-----------------------------------------------------------------------------
// File: CommonPlayerControllerComponent_Client.cpp
// 클라이언트용 구현(OwnerT = CGameObject)
// - Object.h를 include 해서 GetComponent<T>() 템플릿이 보이게 함
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "CommonPlayerControllerComponent.h"
#include "Object.h"       
#include "Component.h" 

static GameMath::Vec3 ToGM(const XMFLOAT3& v) { return GameMath::Vec3(v.x, v.y, v.z); }
static XMFLOAT3 ToDX(const GameMath::Vec3& v) { return XMFLOAT3(v.x, v.y, v.z); }

CCommonPlayerControllerComponent::CCommonPlayerControllerComponent(OwnerT* owner)
    : CComponentT<CCommonPlayerControllerComponent>(owner)
{
}

void CCommonPlayerControllerComponent::SetYawDegrees(float yawDeg)
{
    m_yawDeg = GameMath::NormalizeYaw(yawDeg);
    ApplyYawToOwnerTransform();
}

void CCommonPlayerControllerComponent::ApplyYawToOwnerTransform()
{
    OwnerT* owner = GetOwner();
    if (!owner) return;

    if (auto* tr = owner->GetComponent<CTransformComponent>())
        tr->SetYawDegrees(m_yawDeg);
}

void CCommonPlayerControllerComponent::SetInputDirection(int32_t direction)
{
    m_inputDir = direction;
}

void CCommonPlayerControllerComponent::Move(
    int32_t direction,
    float distance,
    bool bUpdateVelocity,
    EVerticalMoveSpace /*upSpace*/)
{
    OwnerT* owner = GetOwner();
    if (!owner || !direction) return;

    GameMath::Vec3 moveDir = PlayerLogic::ComputeMoveDirection(direction, m_yawDeg);
    GameMath::Vec3 shift = moveDir * distance;

    if (direction & PlayerLogic::PL_DIR_UP)   shift.y += distance;
    if (direction & PlayerLogic::PL_DIR_DOWN) shift.y -= distance;

    MoveShift(shift, bUpdateVelocity);
}

void CCommonPlayerControllerComponent::MoveShift(const GameMath::Vec3& shift, bool bUpdateVelocity)
{
    OwnerT* owner = GetOwner();
    if (!owner) return;

    if (bUpdateVelocity)
    {
        m_velocity += shift;
    }
    else
    {
        if (auto* tr = owner->GetComponent<CTransformComponent>())
            tr->Translate(ToDX(shift));
    }
}

void CCommonPlayerControllerComponent::Rotate(float /*pitchDeg*/, float yawDeg, float /*rollDeg*/)
{
    if (yawDeg != 0.0f)
        SetYawDegrees(m_yawDeg + yawDeg);
}

void CCommonPlayerControllerComponent::OnUpdate(float dt)
{
    OwnerT* owner = GetOwner();
    if (!owner) return;

    auto* tr = owner->GetComponent<CTransformComponent>();
    if (!tr) return;

    GameMath::Vec3 pos = ToGM(tr->position);

    PlayerLogic::ApplyPhysics(
        pos,
        m_velocity,
        m_gravity,
        m_friction,
        m_maxVelXZ,
        m_maxVelY,
        dt
    );

    tr->SetPosition(ToDX(pos));
}

NetworkPlayerState CCommonPlayerControllerComponent::ToNetworkState(uint64_t playerId) const
{
    OwnerT* owner = GetOwner();
    GameMath::Vec3 pos = GameMath::Vec3::Zero();

    if (owner)
    {
        if (auto* tr = owner->GetComponent<CCommonTransformComponent>())
            pos = tr->GetPosition();
    }

    uint8_t animState = (m_inputDir != 0) ? AnimStateType::Run : AnimStateType::Idle;
    return NetworkPlayerState::FromGameMath(playerId, pos, m_yawDeg, animState, 0.f);
}

void CCommonPlayerControllerComponent::ApplyNetworkState(const NetworkPlayerState& state)
{
    OwnerT* owner = GetOwner();
    if (!owner) return;

    if (auto* tr = owner->GetComponent<CCommonTransformComponent>())
    {
        tr->SetPosition(state.transform.ToVec3());
        tr->SetYawDegrees(state.transform.yaw);
    }

    m_yawDeg = state.transform.yaw;
    m_inputDir = state.inputKeys;
}