//-----------------------------------------------------------------------------
// File: PlayerController.cpp
// 서버/클라 공용 플레이어 컨트롤러 구현
//-----------------------------------------------------------------------------
#include "pch.h"
#include "PlayerController.h"
#include "ServerObject.h"

CPlayerControllerComponent::CPlayerControllerComponent(CServerObject* owner)
    : CComponentT<CPlayerControllerComponent>(owner)
{
}

void CPlayerControllerComponent::SetYawDegrees(float yawDeg)
{
    m_yawDeg = GameMath::NormalizeYaw(yawDeg);
    ApplyYawToOwnerTransform();
}

void CPlayerControllerComponent::ApplyYawToOwnerTransform()
{
    CServerObject* owner = GetOwner();
    if (!owner) return;

    if (auto* tr = owner->GetComponent<CTransformComponent>())
        tr->SetYawDegrees(m_yawDeg);
}

void CPlayerControllerComponent::SetInputDirection(int32_t direction)
{
    m_inputDir = direction;
}

void CPlayerControllerComponent::Move(
    int32_t direction,
    float distance,
    bool bUpdateVelocity,
    EVerticalMoveSpace /*upSpace*/)
{
    CServerObject* owner = GetOwner();
    if (!owner || !direction) return;

    // PlayerLogic으로 이동 방향 계산
    GameMath::Vec3 moveDir = PlayerLogic::ComputeMoveDirection(direction, m_yawDeg);
    GameMath::Vec3 shift = moveDir * distance;

    // 수직 이동
    if (direction & PlayerLogic::DIR_UP)
        shift.y += distance;
    if (direction & PlayerLogic::DIR_DOWN)
        shift.y -= distance;

    MoveShift(shift, bUpdateVelocity);
}

void CPlayerControllerComponent::MoveShift(const GameMath::Vec3& shift, bool bUpdateVelocity)
{
    CServerObject* owner = GetOwner();
    if (!owner) return;

    if (bUpdateVelocity)
    {
        m_velocity += shift;
    }
    else
    {
        if (auto* tr = owner->GetComponent<CTransformComponent>())
            tr->Translate(shift);
    }
}

void CPlayerControllerComponent::Rotate(float /*pitchDeg*/, float yawDeg, float /*rollDeg*/)
{
    if (yawDeg != 0.0f)
        SetYawDegrees(m_yawDeg + yawDeg);
}

void CPlayerControllerComponent::OnUpdate(float dt)
{
    CServerObject* owner = GetOwner();
    if (!owner) return;

    auto* tr = owner->GetComponent<CTransformComponent>();
    if (!tr) return;

    // 현재 위치 가져오기
    GameMath::Vec3 pos = tr->GetPosition();

    // ★ 공유 로직 호출 ★
    PlayerLogic::ApplyPhysics(
        pos,
        m_velocity,
        m_gravity,
        m_friction,
        m_maxVelXZ,
        m_maxVelY,
        dt
    );

    // 위치 적용
    tr->SetPosition(pos);
}

NetworkPlayerState CPlayerControllerComponent::ToNetworkState(uint64_t playerId) const
{
    CServerObject* owner = GetOwner();
    GameMath::Vec3 pos = GameMath::Vec3::Zero();
    
    if (owner)
    {
        if (auto* tr = owner->GetComponent<CTransformComponent>())
            pos = tr->GetPosition();
    }

    // 애니메이션 상태: 이동 중이면 Run, 아니면 Idle
    uint8_t animState = (m_inputDir != 0) ? AnimStateType::Run : AnimStateType::Idle;

    return NetworkPlayerState::FromGameMath(playerId, pos, m_yawDeg, animState, 0.f);
}

void CPlayerControllerComponent::ApplyNetworkState(const NetworkPlayerState& state)
{
    CServerObject* owner = GetOwner();
    if (!owner) return;

    if (auto* tr = owner->GetComponent<CTransformComponent>())
    {
        tr->SetPosition(state.transform.ToVec3());
        tr->SetYawDegrees(state.transform.yaw);
    }

    m_yawDeg = state.transform.yaw;
    m_inputDir = state.inputKeys;
}