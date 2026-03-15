#pragma once
//-----------------------------------------------------------------------------
// File: ServerObject.h
// 서버용 게임 오브젝트 기본 클래스
// DirectX 의존성 없음
//-----------------------------------------------------------------------------

#ifndef COMMON_OWNER_TYPE
#define COMMON_OWNER_TYPE CServerObject
#endif

namespace Protocol
{
    enum AnimationType;
}

#include <vector>
#include <memory>
#include <cstdint>

#include "GameMath.h"
#include "GameTypes.h"
#include "BaseComponent.h"
#include "CTransformComponent.h"

//-----------------------------------------------------------------------------
// CServerObject: 서버용 게임 오브젝트
// - Transform (위치, 회전)
// - 컴포넌트 시스템
// - 애니메이션 상태 (렌더링 없음)
//-----------------------------------------------------------------------------
class CServerObject
{
public:
    CServerObject();
    virtual ~CServerObject();

    // ========================================
    // Transform (편의 메서드)
    // ========================================
    void SetPosition(float x, float y, float z);
    void SetPosition(const GameMath::Vec3& pos);
    GameMath::Vec3 GetPosition() const;

    void SetYaw(float degrees);
    float GetYaw() const;

    void Rotate(float pitchDeg, float yawDeg, float rollDeg);

    GameMath::Vec3 GetLook() const;
    GameMath::Vec3 GetRight() const;
    GameMath::Vec3 GetUp() const;

    // ========================================
    // Movement
    // ========================================
    void MoveForward(float distance);
    void MoveStrafe(float distance);
    void MoveUp(float distance);
    void Move(const GameMath::Vec3& shift);

    // ========================================
    // Velocity / Physics State
    // ========================================
    void SetVelocity(const GameMath::Vec3& v) { m_velocity = v; }
    const GameMath::Vec3& GetVelocity() const { return m_velocity; }

    void SetGravity(const GameMath::Vec3& g) { m_gravity = g; }
    void SetFriction(float f) { m_friction = f; }
    void SetMaxVelocityXZ(float v) { m_maxVelXZ = v; }
    void SetMaxVelocityY(float v) { m_maxVelY = v; }

    // ========================================
    // Update
    // ========================================
    virtual void Update(uint32 serverTick);
    virtual void Animate(float dt);

    // ========================================
    // Animation State
    // ========================================
    void SetAnimState(Protocol::AnimationType state);
    Protocol::AnimationType GetAnimState() const { return m_animState; }

    void SetAnimTick(int t) { m_animTick = t; }
    int GetAnimTick() const { return m_animTick; }

    // ========================================
    // ID / Type
    // ========================================
    void SetObjectId(uint64_t id) { m_objectId = id; }
    uint64_t GetObjectId() const { return m_objectId; }

    void SetObjectType(uint32_t type) { m_objectType = type; }
    uint32_t GetObjectType() const { return m_objectType; }

    // ========================================
    // Component System
    // ========================================
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args);

    template<typename T>
    T* GetComponent() const;

    void CreateComponents();
    void DestroyComponents();
    void UpdateComponents(float dt);

    // ========================================
    // Network State
    // ========================================
    NetworkPlayerState ToNetworkState() const;
    void ApplyNetworkState(const NetworkPlayerState& state);


	// ========================================
	// Active State (옵션)
private:
	bool active = false; // 활성화 여부 (옵션)
public:
    bool IsActive() const { return active; }
	void SetActive(bool b) { active = b; }
    // ========================================



	// ========================================
	// Transform Component (항상 존재)
	// ========================================

public:
    // Animation state
    Protocol::AnimationType m_animState = Protocol::ANIMATION_TYPE_IDLE;
    int m_animTick = 0;

protected:
    // Transform 컴포넌트 (항상 존재)
    CCommonTransformComponent* m_pTransform = nullptr;

    // Physics (컴포넌트 없이 직접 관리할 때 사용)
    GameMath::Vec3 m_velocity = GameMath::Vec3::Zero();
    GameMath::Vec3 m_gravity = GameMath::Vec3::Zero();
    float m_friction = 0.f;
    float m_maxVelXZ = 0.f;
    float m_maxVelY = 0.f;



    // Identity
    uint64_t m_objectId = 0;
    uint32_t m_objectType = 0;

    // Components
    std::vector<std::unique_ptr<CComponent>> m_components;
    bool m_bComponentsCreated = false;

    // Physics helper
    void ApplyPhysics(float dt);    

};

//-----------------------------------------------------------------------------
// Template implementations
//-----------------------------------------------------------------------------
template<typename T, typename... Args>
T* CServerObject::AddComponent(Args&&... args)
{
    auto comp = std::make_unique<T>(this, std::forward<Args>(args)...);
    T* raw = comp.get();
    m_components.push_back(std::move(comp));
    return raw;
}

template<typename T>
T* CServerObject::GetComponent() const
{
    for (auto& c : m_components)
    {
        if (c && c->GetTypeId() == CComponent::StaticTypeId<T>())
            return static_cast<T*>(c.get());
    }
    return nullptr;
}
