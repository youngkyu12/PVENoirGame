#pragma once
#include "Component.h"
#include <vector>
#include <unordered_map>
#include <string>

class CColliderComponent;

class CCollisionSystem
{
public:
    explicit CCollisionSystem();

    void OnUpdate();

    // Scene/오브젝트 생성/삭제 시 호출해주면 됨
    void RegisterCollider(CColliderComponent* c);
    void UnregisterCollider(CColliderComponent* c);
	void SetBoundingFrustum(const BoundingFrustum& mCameraCollider);

	size_t GetColiidersNum() const;
	const std::vector<CColliderComponent*>& GetColliders() const;
private:
    bool PassFilter(const CColliderComponent* a, const CColliderComponent* b) const;
    void HandlePair(CColliderComponent* a, CColliderComponent* b);
	bool IsVisible(const BoundingFrustum& frustum, const CColliderComponent* collider);

private:
    std::vector<CColliderComponent*> mColliders;
	BoundingFrustum mCameraCollider;
};