//-----------------------------------------------------------------------------
// File: PlayerController.h
// 서버/클라 공용 플레이어 컨트롤러 컴포넌트
// DirectX 의존성 없음 - PlayerLogic 사용
//-----------------------------------------------------------------------------
#pragma once
#include "BaseComponent.h"
#include "CTransformComponent.h"
#include "PlayerLogic.h"
#include "GameTypes.h"

class CServerObject;

class CPlayerControllerComponent final : public CComponentT<CPlayerControllerComponent>
{
public:
    enum class EVerticalMoveSpace : uint8_t
    {
        WorldUp,
        LocalUp
    };

public:
    explicit CPlayerControllerComponent(CServerObject* owner);

    // ----------------------------
    // Input
    // ----------------------------
    void SetInputDirection(int32_t direction);
    void SetInputDirection(uint32_t direction) { SetInputDirection(static_cast<int32_t>(direction)); }
    int32_t GetInputDirection() const { return m_inputDir; }

    // ----------------------------
    // Movement API
    // ----------------------------
    void Move(int32_t direction, float distance, bool bUpdateVelocity = false,
              EVerticalMoveSpace upSpace = EVerticalMoveSpace::WorldUp);

    void MoveShift(const GameMath::Vec3& shift, bool bUpdateVelocity = false);

    void Rotate(float pitchDeg, float yawDeg, float rollDeg);

    // ----------------------------
    // Physics Parameters
    // ----------------------------
    void SetFriction(float f) { m_friction = f; }
    float GetFriction() const { return m_friction; }

    void SetGravity(const GameMath::Vec3& g) { m_gravity = g; }
    const GameMath::Vec3& GetGravity() const { return m_gravity; }

    void SetMaxVelocityXZ(float v) { m_maxVelXZ = v; }
    float GetMaxVelocityXZ() const { return m_maxVelXZ; }

    void SetMaxVelocityY(float v) { m_maxVelY = v; }
    float GetMaxVelocityY() const { return m_maxVelY; }

    void SetMoveSpeed(float s) { m_moveSpeed = s; }
    float GetMoveSpeed() const { return m_moveSpeed; }

    void SetVelocity(const GameMath::Vec3& v) { m_velocity = v; }
    const GameMath::Vec3& GetVelocity() const { return m_velocity; }

    // ----------------------------
    // Rotation
    // ----------------------------
    void SetYawDegrees(float yawDeg);
    float GetYawDegrees() const { return m_yawDeg; }

    // ----------------------------
    // Lifecycle
    // ----------------------------
    void OnUpdate(float dt) override;

    // ----------------------------
    // Network State
    // ----------------------------
    NetworkPlayerState ToNetworkState(uint64_t playerId) const;
    void ApplyNetworkState(const NetworkPlayerState& state);

private:
    void ApplyYawToOwnerTransform();

private:
    // Physics state
    GameMath::Vec3 m_velocity = GameMath::Vec3::Zero();
    GameMath::Vec3 m_gravity = GameMath::Vec3::Zero();

    // Parameters
    float m_yawDeg = 0.f;
    float m_maxVelXZ = 0.f;
    float m_maxVelY = 0.f;
    float m_friction = 0.f;
    float m_moveSpeed = 50.f;

    // Input state
    int32_t m_inputDir = 0;
};
