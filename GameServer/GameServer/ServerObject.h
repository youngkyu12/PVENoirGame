#pragma once
//-----------------------------------------------------------------------------
// File: ServerObject.h
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
// CServerObject
//-----------------------------------------------------------------------------
class CServerObject
{
public:
    CServerObject();
    virtual ~CServerObject();

    // ========================================
    // Transform
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
    // HP / Damage
    // ========================================
    void SetMaxHp(int hp, bool fillCurrent = true)
    {
        m_maxHp = (hp < 1) ? 1 : hp;
        if (fillCurrent) m_currentHp = m_maxHp;
    }
    int  GetMaxHp()     const { return m_maxHp; }
    int  GetCurrentHp() const { return m_currentHp; }
    bool IsDead()       const { return m_currentHp <= 0; }

    bool TakeDamage(int damage)
    {
        if (damage <= 0 || IsDead()) return false;
        m_currentHp -= damage;
        if (m_currentHp < 0) m_currentHp = 0;
        return true;
    }

    void ResetHpToMax() { m_currentHp = m_maxHp; }

    void SetAttackPower(int ap) { m_attackPower = ap; }
    int  GetAttackPower() const { return m_attackPower; }

    // ========================================
    // Active State
private:
    bool active = false;
public:
    bool IsActive() const { return active; }
    void SetActive(bool b) {

        if (m_objectId == 0 && b != active)
            cout << m_objectId << " goes to " << b
            << " from " << active
            << std::endl;

        active = b;
    }
    // ========================================



    // ========================================
    // Transform Component
    // ========================================

public:
    // Animation state
    Protocol::AnimationType m_animState = Protocol::ANIMATION_TYPE_IDLE;
    int m_animTick = 0;

protected:
    CCommonTransformComponent* m_pTransform = nullptr;

    // Physics
    GameMath::Vec3 m_velocity = GameMath::Vec3::Zero();
    GameMath::Vec3 m_gravity = GameMath::Vec3::Zero();
    float m_friction = 0.f;
    float m_maxVelXZ = 0.f;
    float m_maxVelY = 0.f;

    // HP
    int m_maxHp = 1;
    int m_currentHp = 1;
    int m_attackPower = 0;

    // Identity
    uint64_t m_objectId = 0;
    uint32_t m_objectType = 0;

    // Components
    std::vector<std::unique_ptr<CComponent>> m_components;
    bool m_bComponentsCreated = false;

    // Physics helper
    void ApplyPhysics(float dt);

    bool m_bpassive = true;
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