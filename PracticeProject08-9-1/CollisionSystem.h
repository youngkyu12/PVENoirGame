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

	void RegisterCollider(CColliderComponent* c);
	void UnregisterCollider(CColliderComponent* c);

	bool HasCollisionWithWorldStatic(const CColliderComponent* subject) const;

private:
	bool PassFilter(const CColliderComponent* a, const CColliderComponent* b) const;
	bool IsPairIntersecting(const CColliderComponent* a, const CColliderComponent* b) const;
	void HandlePair(CColliderComponent* a, CColliderComponent* b);

private:
	std::vector<CColliderComponent*> mColliders;
};