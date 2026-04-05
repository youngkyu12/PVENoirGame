#pragma once

#include "BaseComponent.h"
#include <vector>

class CColliderComponent;

class CCollisionSystem
{
public:
	explicit CCollisionSystem();

	void OnUpdate();

	void RegisterCollider(CColliderComponent* c);
	void UnregisterCollider(CColliderComponent* c);

	bool HasCollisionWithWorldStatic(const CColliderComponent* subject) const;

	size_t GetCollidersNum() const;
	size_t GetColiidersNum() const { return GetCollidersNum(); }
	const std::vector<CColliderComponent*>& GetColliders() const;

private:
	bool PassFilter(const CColliderComponent* a, const CColliderComponent* b) const;
	bool IsPairIntersecting(const CColliderComponent* a, const CColliderComponent* b) const;
	void HandlePair(CColliderComponent* a, CColliderComponent* b);

private:
	std::vector<CColliderComponent*> mColliders;
};
