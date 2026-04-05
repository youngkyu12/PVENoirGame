#include "pch.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"
#include "ServerObject.h"

namespace
{
	enum : uint32_t
	{
		kCollisionLayerWorldStatic = 1
	};
}

CCollisionSystem::CCollisionSystem()
{
}

void CCollisionSystem::RegisterCollider(CColliderComponent* c)
{
	if (!c) return;
	if (std::find(mColliders.begin(), mColliders.end(), c) != mColliders.end())
		return;
	mColliders.push_back(c);
}

void CCollisionSystem::UnregisterCollider(CColliderComponent* c)
{
	if (!c) return;
	auto it = std::remove(mColliders.begin(), mColliders.end(), c);
	mColliders.erase(it, mColliders.end());
}

size_t CCollisionSystem::GetCollidersNum() const
{
	return mColliders.size();
}

const std::vector<CColliderComponent*>& CCollisionSystem::GetColliders() const
{
	return mColliders;
}

bool CCollisionSystem::PassFilter(const CColliderComponent* a, const CColliderComponent* b) const
{
	const uint32_t bitA = (1u << a->GetLayer());
	const uint32_t bitB = (1u << b->GetLayer());

	if ((a->GetMask() & bitB) == 0) return false;
	if ((b->GetMask() & bitA) == 0) return false;
	return true;
}

bool CCollisionSystem::IsPairIntersecting(const CColliderComponent* a, const CColliderComponent* b) const
{
	if (!a || !b)
		return false;

	if (!PassFilter(a, b))
		return false;

	if (a->GetType() == EColliderType::BCapsule && b->GetType() == EColliderType::BCapsule)
		return a->GetBCapsule().Intersects(b->GetBCapsule());

	if (a->GetType() == EColliderType::BCapsule && b->GetType() == EColliderType::OOBB)
		return b->IntersectsCapsuleHierarchical(a->GetBCapsule());

	if (a->GetType() == EColliderType::OOBB && b->GetType() == EColliderType::BCapsule)
		return a->IntersectsCapsuleHierarchical(b->GetBCapsule());

	return false;
}

bool CCollisionSystem::HasCollisionWithWorldStatic(const CColliderComponent* subject) const
{
	if (!subject)
		return false;

	for (CColliderComponent* other : mColliders)
	{
		if (!other) continue;
		if (other == subject) continue;
		if (other->GetLayer() != kCollisionLayerWorldStatic) continue;

		if (IsPairIntersecting(subject, other))
			return true;
	}

	return false;
}

void CCollisionSystem::HandlePair(CColliderComponent* a, CColliderComponent* b)
{
	if (!a || !b)
		return;

	if (!IsPairIntersecting(a, b))
		return;

	if (a->IsTrigger() || b->IsTrigger())
		return;

	const bool isCapsuleVsOOBB =
		(a->GetType() == EColliderType::BCapsule && b->GetType() == EColliderType::OOBB) ||
		(a->GetType() == EColliderType::OOBB && b->GetType() == EColliderType::BCapsule);

	if (isCapsuleVsOOBB)
		return;

	CColliderComponent* pushedCollider = nullptr;
	if (a->GetType() == EColliderType::BCapsule)
		pushedCollider = a;
	else if (b->GetType() == EColliderType::BCapsule)
		pushedCollider = b;

	if (!pushedCollider)
		return;

	auto* owner = pushedCollider->GetOwner();
	if (!owner)
		return;

	auto* transform = owner->GetComponent<CCommonTransformComponent>();
	if (!transform)
		return;

	const float pushBackDistance = 0.1f;
	GameMath::Vec3 forward = transform->GetLook();

	if (forward.LengthSq() < 1e-8f)
		forward = GameMath::Vec3::Forward();

	transform->Translate((-pushBackDistance) * forward);
}

void CCollisionSystem::OnUpdate()
{
	const size_t n = mColliders.size();
	for (size_t i = 0; i < n; ++i)
	{
		auto* a = mColliders[i];
		if (!a) continue;

		for (size_t j = i + 1; j < n; ++j)
		{
			auto* b = mColliders[j];
			if (!b) continue;
			HandlePair(a, b);
		}
	}
}