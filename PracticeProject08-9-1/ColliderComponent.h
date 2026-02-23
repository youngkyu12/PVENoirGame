#pragma once
#include "Component.h"
#include <cstdint>
#include <unordered_set>
#include <vector>

class CTransformComponent;

struct AABB
{
    XMFLOAT3 min = XMFLOAT3(0, 0, 0);
    XMFLOAT3 max = XMFLOAT3(0, 0, 0);
};

enum class EColliderType : uint8_t
{
    None = 0,
    Sphere,
    AABB
};

class CColliderComponent final : public CComponentT<CColliderComponent>
{
public:
    explicit CColliderComponent(CGameObject* owner);

    void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;
    void OnUpdate(float dt) override;

    // Shape setup
    void SetSphere(float radius, const XMFLOAT3& localCenter = XMFLOAT3(0, 0, 0));
    void SetAABB(const XMFLOAT3& halfExtents, const XMFLOAT3& localCenter = XMFLOAT3(0, 0, 0));

    EColliderType GetType() const { return mType; }

    // Filtering
    void SetLayer(uint32_t layer) { mLayer = layer; }
    void SetMask(uint32_t mask) { mMask = mask; }
    uint32_t GetLayer() const { return mLayer; }
    uint32_t GetMask() const { return mMask; }

    void SetTrigger(bool v) { mIsTrigger = v; }
    bool IsTrigger() const { return mIsTrigger; }

    // World data
    const XMFLOAT3& GetWorldCenter() const { return mWorldCenter; }
    float GetWorldRadius() const { return mWorldRadius; }
    const AABB& GetWorldAABB() const { return mWorldAABB; }

    // Pair test (MVP)
    bool Intersects(const CColliderComponent& other) const;

    // Collision events (Scene에서 호출)
    virtual void OnCollisionEnter(CColliderComponent* other) {}
    virtual void OnCollisionExit(CColliderComponent* other) {}

    virtual void OnTriggerEnter(CColliderComponent* other) {}
    virtual void OnTriggerExit(CColliderComponent* other) {}

    // 내부용: 접촉상태 관리
    bool WasOverlapping(CColliderComponent* other) const;
    void MarkOverlapping(CColliderComponent* other);
    void UnmarkOverlapping(CColliderComponent* other);

private:
    void UpdateWorldBounds();

private:
    CTransformComponent* mTransform = nullptr;

    // Shape (local)
    EColliderType mType = EColliderType::None;
    XMFLOAT3      mLocalCenter = XMFLOAT3(0, 0, 0);

    // Sphere
    float         mRadius = 0.5f;

    // AABB (rotation 없는 axis-aligned)
    XMFLOAT3      mHalfExtents = XMFLOAT3(0.5f, 0.5f, 0.5f);

    // World cache
    XMFLOAT3      mWorldCenter = XMFLOAT3(0, 0, 0);
    float         mWorldRadius = 0.5f;
    AABB          mWorldAABB;

    // Filtering
    uint32_t      mLayer = 0;                // 0..31 권장
    uint32_t      mMask = 0xFFFFFFFFu;      // 허용 레이어 bitmask
    bool          mIsTrigger = false;

    // overlap tracking (Enter/Exit 판정용)
    std::unordered_set<CColliderComponent*> mOverlaps;
};