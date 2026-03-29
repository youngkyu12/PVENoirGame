#pragma once
#include "Component.h"
#include "ModelComponent.h"
#include "RendererComponent.h"
#include <vector>
#include <unordered_map>
#include <string>
#include "BoundingCapsule.h"
class CTransformComponent;



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
    BoundingBox GetAABB() const { return WorldAABB; }
    BoundingOrientedBox GetOOBB() const { return WorldOOBB; }
    BoundingSphere GetBSphere() const { return WorldBSphere; }
    BoundingCapsule GetBCapsule() const { return WorldBCapsule; }
    vector<BoundingCapsule> GetSubBCapsules() const { return WorldSubBCapsules; }

    // Filtering
    void SetLayer(uint32_t layer) { mLayer = layer; }
    void SetMask(uint32_t mask) { mMask = mask; }
    uint32_t GetLayer() const { return mLayer; }
    uint32_t GetMask() const { return mMask; }
    bool IsTrigger() const { return mIsTrigger; }
	void DisabledRender();

    void UpdateWorldBounds();

private:
    CTransformComponent* mTransform = nullptr;
    CModelComponent* mModel = nullptr;
	CRendererComponent* mRender = nullptr;

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