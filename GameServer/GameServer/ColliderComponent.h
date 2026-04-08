#pragma once

#include "BaseComponent.h"
#include "CTransformComponent.h"
#include "BoundingCapsule.h"

enum class EColliderType
{
	None = 0,
	AABB,
	OOBB,
	BSphere,
	BCapsule
};

class CColliderComponent final : public CComponentT<CColliderComponent>
{
public:
	using OwnerT = CComponent::OwnerT;
	explicit CColliderComponent(OwnerT* owner, EColliderType type);

	void OnCreate() override;
	void OnUpdate(float dt) override;

	void SetAABB(const XMFLOAT3& Min, const XMFLOAT3& Max);
	void SetOOBB(const XMFLOAT3& Min, const XMFLOAT3& Max);
  void SetOOBB(const BoundingOrientedBox& localOOBB);
  void SetSubOOBBs(const std::vector<BoundingOrientedBox>& localSubOOBBs);
	void ClearSubOOBBs();
	void SetBSphere(const XMFLOAT3& Min, const XMFLOAT3& Max);
	void SetBCapsule(const XMFLOAT3& Min, const XMFLOAT3& Max);
	void SetSubBCapsule(const XMFLOAT3& Min, const XMFLOAT3& Max);

	EColliderType GetType() const { return mColliderType; }
	const BoundingBox& GetAABB() const { return WorldAABB; }
	const BoundingOrientedBox& GetOOBB() const { return WorldOOBB; }
   const std::vector<BoundingOrientedBox>& GetSubOOBBs() const { return WorldSubOOBBs; }
	const BoundingSphere& GetBSphere() const { return WorldBSphere; }
	const BoundingCapsule& GetBCapsule() const { return WorldBCapsule; }
	const std::vector<BoundingCapsule>& GetSubBCapsules() const { return WorldSubBCapsules; }

	bool IntersectsCapsuleHierarchical(const BoundingCapsule& capsule) const;

	void SetLayer(uint32_t layer) { mLayer = layer; }
	void SetMask(uint32_t mask) { mMask = mask; }
	uint32_t GetLayer() const { return mLayer; }
	uint32_t GetMask() const { return mMask; }
	bool IsTrigger() const { return mIsTrigger; }

	void UpdateWorldBounds();

private:
	CCommonTransformComponent* mTransform = nullptr;
	EColliderType mColliderType = EColliderType::None;

	BoundingBox LocalAABB{};
	BoundingOrientedBox LocalOOBB{};
  std::vector<BoundingOrientedBox> LocalSubOOBBs;
	BoundingSphere LocalBSphere{};
	BoundingCapsule LocalBCapsule{};
	std::vector<BoundingCapsule> LocalSubBCapsules;

	BoundingBox WorldAABB{};
	BoundingOrientedBox WorldOOBB{};
  std::vector<BoundingOrientedBox> WorldSubOOBBs;
	BoundingSphere WorldBSphere{};
	BoundingCapsule WorldBCapsule{};
	std::vector<BoundingCapsule> WorldSubBCapsules;

	uint32_t mLayer = 0;
	uint32_t mMask = 0xFFFFFFFFu;
	bool mIsTrigger = false;
};
