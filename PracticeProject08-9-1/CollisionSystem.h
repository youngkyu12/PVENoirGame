#pragma once
#include "Component.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>

class CColliderComponent;
class CGameObject;

class CCollisionSystem
{
public:
	using HitEffectCallback =
		std::function<void(CGameObject* weaponObject, CGameObject* targetObject)>;

public:
	explicit CCollisionSystem();

	void SetHitEffectCallback(HitEffectCallback callback);

	void OnUpdate();

	void RegisterCollider(CColliderComponent* c);
	void UnregisterCollider(CColliderComponent* c);

	bool HasCollisionWithWorldStatic(const CColliderComponent* subject) const;

	size_t GetColiidersNum() const;
	const std::vector<CColliderComponent*>& GetColliders() const;

private:
	bool PassFilter(const CColliderComponent* a, const CColliderComponent* b) const;
	bool IsPairIntersecting(const CColliderComponent* a, const CColliderComponent* b) const;
	void HandlePair(CColliderComponent* a, CColliderComponent* b);

private:
	std::vector<CColliderComponent*> mColliders;

	HitEffectCallback mHitEffectCallback;
};