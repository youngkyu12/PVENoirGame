#pragma once
#include "Component.h"
#include "ModelComponent.h"
#include "RendererComponent.h"
#include <vector>
#include <unordered_map>
#include <string>
#include "BoundingCapsule.h"
class CTransformComponent;

struct MeshOOBBSet
{
	BoundingOrientedBox LocalMeshOOBB;
	BoundingOrientedBox WorldMeshOOBB;

	vector<BoundingOrientedBox> LocalSubOOBBs;
	vector<BoundingOrientedBox> WorldSubOOBBs;
};

struct AuthoredSubMeshOOBB
{
	XMFLOAT3 Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT4 RotationQuat = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	XMFLOAT3 Size = XMFLOAT3(0.0f, 0.0f, 0.0f);
};

struct BoneCapsuleLink
{
	int parentBoneIndex = -1;
	int childBoneIndex = -1;
	float radius = 0.0f;
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
	BoundingBox GetAABB() const { return WorldAABB; }
	BoundingOrientedBox GetOOBB() const { return WorldOOBB; }
	BoundingSphere GetBSphere() const { return WorldBSphere; }
	BoundingCapsule GetBCapsule() const { return WorldBCapsule; }
	vector<BoundingCapsule> GetSubBCapsules() const { return WorldSubBCapsules; }

	const vector<MeshOOBBSet>& GetMeshOOBBSets() const { return mMeshOOBBSets; }
	void SetStaticSubMeshAuthoredOOBBs(
	const std::unordered_map<std::string, std::vector<AuthoredSubMeshOOBB>>& authoredOOBBs)
	{
		mStaticSubMeshAuthoredOOBBs = authoredOOBBs;
	}
	bool IntersectsCapsuleHierarchical(const BoundingCapsule& capsule) const;

	// Filtering
	void SetLayer(uint32_t layer) { mLayer = layer; }
	void SetMask(uint32_t mask) { mMask = mask; }
	uint32_t GetLayer() const { return mLayer; }
	uint32_t GetMask() const { return mMask; }
	bool IsTrigger() const { return mIsTrigger; }

	// external helpers
	void UpdateWorldBounds();
	void DisabledRender();

	const std::vector<BoundingCapsule>& GetBoneCapsules() const { return mWorldBoneCapsules; }
	bool HasBoneCapsules() const { return !mWorldBoneCapsules.empty(); }

	bool IntersectsBoneCapsulesHierarchical(const BoundingOrientedBox& box) const;
	bool IntersectsBoneCapsulesHierarchical(const BoundingCapsule& capsule) const;

	void BuildBoneCapsulesFromSkeleton();
	void UpdateBoneCapsulesFromCurrentPose();

	void OnLateUpdate(float dt) override;

	void SetCollisionEnabled(bool enabled) { mCollisionEnabled = enabled; }
	bool IsCollisionEnabled() const { return mCollisionEnabled; }

private:
	static BoundingOrientedBox MakeLocalOOBB(const XMFLOAT3& Min, const XMFLOAT3& Max);
	static BoundingOrientedBox MakeAuthoredLocalOOBB(
	const XMFLOAT3& Center,
	const XMFLOAT4& RotationQuat,
	const XMFLOAT3& Size);
	static BoundingOrientedBox MakeLocalOOBBFromMatrix(const XMFLOAT4X4& unitBoxToLocal);
	void BuildHierarchicalOOBBs(const vector<shared_ptr<CMesh>>& meshes);

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

	// mesh/submesh 계층형 OOBB
	vector<MeshOOBBSet> mMeshOOBBSets;
	std::unordered_map<std::string, std::vector<AuthoredSubMeshOOBB>> mStaticSubMeshAuthoredOOBBs;
	
	// Filtering
	uint32_t mLayer = 0;
	uint32_t mMask = 0xFFFFFFFFu;
	bool mIsTrigger = false;

	bool mCollisionEnabled = true;
private:
	std::vector<BoneCapsuleLink> mBoneCapsuleLinks;
	std::vector<BoundingCapsule> mWorldBoneCapsules;

public:
	void SetWeaponBoneCapsuleRoots(const std::vector<std::string>& rootBoneNames);
	void ClearWeaponBoneCapsuleRoots();

	void SetWeaponCapsulesActive(bool active) { mWeaponBoneCapsulesActive = active; }
	bool AreWeaponCapsulesActive() const { return mWeaponBoneCapsulesActive; }

	const std::vector<BoundingCapsule>& GetWeaponBoneCapsules() const { return mWorldWeaponBoneCapsules; }
	bool HasWeaponBoneCapsules() const { return !mWorldWeaponBoneCapsules.empty(); }

	bool IntersectsActiveWeaponBoneCapsulesAgainstBody(const CColliderComponent& targetBody) const;

private:
	void RebuildWeaponBoneCapsuleSelection();

private:
	std::vector<std::string> mWeaponBoneRootNames;
	std::vector<int> mWeaponBoneCapsuleLinkIndices;
	std::vector<BoundingCapsule> mWorldWeaponBoneCapsules;
	bool mWeaponBoneCapsulesActive = false;
};