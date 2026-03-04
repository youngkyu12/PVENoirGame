#pragma once
#include "Component.h"
#include "ModelComponent.h"
#include <vector>
#include <unordered_map>
#include <string>

class CTransformComponent;

struct BoundingCapsule
{
    DirectX::XMFLOAT3 p0{ 0.f, 0.f, 0.f }; // segment start
    DirectX::XMFLOAT3 p1{ 0.f, 0.f, 0.f }; // segment end
    float radius{ 0.1f };

    BoundingCapsule() = default;
    BoundingCapsule(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float r)
        : p0(a), p1(b), radius(r) {
    }
};

class CColliderComponent final : public CComponentT<CColliderComponent>
{
public:
    explicit CColliderComponent(CGameObject* owner, EColliderType Type);

    void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;
    void OnUpdate(float dt) override;

    // Shape setup
    void SetAABB(const XMFLOAT3& Min, const XMFLOAT3& Max);
    void SetOOBB(const XMFLOAT3& Min, const XMFLOAT3& Max);

    EColliderType GetType() const { return mColliderType; }

    // Filtering
    void SetLayer(uint32_t layer) { mLayer = layer; }
    void SetMask(uint32_t mask) { mMask = mask; }
    uint32_t GetLayer() const { return mLayer; }
    uint32_t GetMask() const { return mMask; }

    void SetTrigger(bool v) { mIsTrigger = v; }
    bool IsTrigger() const { return mIsTrigger; }

    // World data
    const XMFLOAT3& GetWorldCenter() const { return Center; }
    float GetWorldRadius() const { return Radius; }
    const BoundingBox& GetWorldAABB() const { return AABB; }

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
    CModelComponent* mModel = nullptr;

    EColliderType mColliderType = EColliderType::None;

    XMFLOAT3      Extents = XMFLOAT3(0.5f, 0.5f, 0.5f);

    // World cache
    XMFLOAT3      Center = XMFLOAT3(0, 0, 0);
    float         Radius = 0.5f;

    // Bounding Box
    BoundingBox AABB;
    BoundingOrientedBox OOBB;
    BoundingSphere BSphere;
    vector<BoundingCapsule> BCapsules;


    // Filtering
    uint32_t      mLayer = 0;                // 0..31 권장
    uint32_t      mMask = 0xFFFFFFFFu;      // 허용 레이어 bitmask
    bool          mIsTrigger = false;

    // overlap tracking (Enter/Exit 판정용)
    std::unordered_set<CColliderComponent*> mOverlaps;
};