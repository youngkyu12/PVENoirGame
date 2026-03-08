#pragma once
#include "Component.h"
#include "ModelComponent.h"
#include <vector>
#include <unordered_map>
#include <string>

class CTransformComponent;

struct BoundingCapsule
{
    XMFLOAT3 p0;
    XMFLOAT3 p1;
    XMFLOAT3 Center;
    float Radius;
    float Height;

    // Creators
    BoundingCapsule() noexcept : p0(0.0f, 0.0f, 0.0f), p1(0.0f, 0.0f, 0.0f), Center(0.0f, 0.0f, 0.0f), Radius(0.1f), Height(0.0f) {}

    BoundingCapsule(const BoundingCapsule&) = default;
    BoundingCapsule& operator=(const BoundingCapsule&) = default;

    BoundingCapsule(BoundingCapsule&&) = default;
    BoundingCapsule& operator=(BoundingCapsule&&) = default;

    constexpr BoundingCapsule(const XMFLOAT3& q0, const XMFLOAT3& q1, const XMFLOAT3& center, const float& radius, const float& height) noexcept
        : p0(q0), p1(q1), Center(center), Radius(radius), Height(height) {
    }

    // Methods
    void Transform(BoundingCapsule& Out, FXMMATRIX M) const noexcept;
    void Transform(BoundingCapsule& Out, float Scale, FXMVECTOR Rotation, FXMVECTOR Translation) const noexcept;
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
    void SetBSphere(const XMFLOAT3& Min, const XMFLOAT3& Max);
    void SetBCapsule(const XMFLOAT3& Min, const XMFLOAT3& Max);
    void SetSubBCapsule(const XMFLOAT3& Min, const XMFLOAT3& Max);
    
    EColliderType GetType() const { return mColliderType; }

    // Filtering
    void SetLayer(uint32_t layer) { mLayer = layer; }
    void SetMask(uint32_t mask) { mMask = mask; }
    uint32_t GetLayer() const { return mLayer; }
    uint32_t GetMask() const { return mMask; }

private:
    void UpdateWorldBounds();

private:
    CTransformComponent* mTransform = nullptr;
    CModelComponent* mModel = nullptr;

    EColliderType mColliderType = EColliderType::None;

    // Local Bounding Box
    BoundingBox LocalAABB;
    BoundingOrientedBox LocalOOBB;
    BoundingSphere LocalBSphere;
    BoundingCapsule LocalBCapsule;
    vector<BoundingCapsule> LocalSubBCapsules;

    // World Bounding Box
    BoundingBox WorldAABB;
    BoundingOrientedBox WorldOOBB;
    BoundingSphere WorldBSphere;
    BoundingCapsule WorldBCapsule;
    vector<BoundingCapsule> WorldSubBCapsules;

    // Filtering
    uint32_t      mLayer = 0;                // 0..31 권장
    uint32_t      mMask = 0xFFFFFFFFu;      // 허용 레이어 bitmask
    bool          mIsTrigger = false;
};