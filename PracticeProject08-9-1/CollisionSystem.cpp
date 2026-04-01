#include "stdafx.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"
#include "Object.h"

#include "ActorTagComponent.h"
#include "Mesh.h"
#include <string>
#include <sstream>

namespace
{
	enum : uint32_t
	{
		kCollisionLayerPlayer = 0,
		kCollisionLayerMonster = 1,
		kCollisionLayerWorldStatic = 2,
		kCollisionLayerPlayerWeapon = 3,
		kCollisionLayerMonsterWeapon = 4
	};

	const char* ColliderTypeToString(EColliderType type)
	{
		switch ( type )
		{
		case EColliderType::AABB:     return "AABB";
		case EColliderType::OOBB:     return "OOBB";
		case EColliderType::BSphere:  return "BSphere";
		case EColliderType::BCapsule: return "BCapsule";
		default:                      return "None";
		}
	}

	bool IsLocalPlayerObject(const CGameObject* obj)
	{
		if ( !obj ) return false;

		auto* tag = obj->GetComponent<CActorTagComponent>();
		if ( !tag ) return false;

		return ( tag->kind == EActorKind::Player &&
				tag->control == EPlayerControl::Local );
	}

	std::string BuildObjectDebugName(const CGameObject* obj, const CColliderComponent* collider)
	{
		std::ostringstream oss;

		if ( !obj )
		{
			oss << "null";
			return oss.str();
		}

		auto* tag = obj->GetComponent<CActorTagComponent>();
		if ( tag )
		{
			if ( tag->kind == EActorKind::Player )
			{
				if ( tag->control == EPlayerControl::Local )
					oss << "LocalPlayer";
				else
					oss << "RemotePlayer";

				oss << "(slot=" << tag->playerSlot << ")";
			}
			else if ( tag->kind == EActorKind::NPC )
			{
				oss << "NPC";
			}
			else
			{
				oss << "Actor";
			}
		}
		else
		{
			oss << "StaticObject";
		}

		if ( collider )
		{
			oss << "[";
			oss << ColliderTypeToString(collider->GetType());
			oss << "]";
		}

		std::shared_ptr<CMesh> mesh = obj->GetMeshShared(0);
		if ( mesh )
		{
			const std::string& src = mesh->GetSourceMeshPath();
			if ( !src.empty() )
			{
				oss << " mesh=" << src;
			}
		}

		oss << " ptr=" << obj;
		return oss.str();
	}

	void DebugPrintCollision(CColliderComponent* a, CColliderComponent* b)
	{
		if ( !a || !b ) return;

		CGameObject* ownerA = a->GetOwner();
		CGameObject* ownerB = b->GetOwner();

		if ( !ownerA || !ownerB ) return;

		// 로컬 플레이어가 포함된 충돌만 출력
		if ( !IsLocalPlayerObject(ownerA) && !IsLocalPlayerObject(ownerB) )
			return;

		std::ostringstream oss;
		oss << "[Collision] "
			<< BuildObjectDebugName(ownerA, a)
			<< " <-> "
			<< BuildObjectDebugName(ownerB, b)
			<< "\n";

		OutputDebugStringA(oss.str().c_str());
	}
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

void CCollisionSystem::SetBoundingFrustum(const BoundingFrustum& BFrustum)
{
	mCameraCollider = BFrustum;
}

size_t CCollisionSystem::GetColiidersNum() const
{
	return mColliders.size();
}

const std::vector<CColliderComponent*>& CCollisionSystem::GetColliders() const
{
	return mColliders;
}

bool CCollisionSystem::PassFilter(const CColliderComponent* a, const CColliderComponent* b) const
{
    // 레이어/마스크: 서로 허용해야 충돌(대칭)
    // b의 layer bit가 a의 mask에 포함 && a의 layer bit가 b의 mask에 포함
    const uint32_t bitA = (1u << a->GetLayer());
    const uint32_t bitB = (1u << b->GetLayer());

    if ((a->GetMask() & bitB) == 0) return false;
    if ((b->GetMask() & bitA) == 0) return false;
    return true;
}

bool CCollisionSystem::IsPairIntersecting(const CColliderComponent* a, const CColliderComponent* b) const
{
	if ( !a || !b )
		return false;

	if ( !PassFilter(a, b) )
		return false;

	const uint32_t layerA = a->GetLayer();
	const uint32_t layerB = b->GetLayer();

	// BCapsule vs BCapsule
	if ( a->GetType() == EColliderType::BCapsule &&
		b->GetType() == EColliderType::BCapsule )
	{
		return a->GetBCapsule().Intersects(b->GetBCapsule());
	}

	// BCapsule vs OOBB
	if ( a->GetType() == EColliderType::BCapsule &&
		b->GetType() == EColliderType::OOBB )
	{
		// body vs world static
		if ( layerB == kCollisionLayerWorldStatic )
			return b->IntersectsCapsuleHierarchical(a->GetBCapsule());

		// body vs weapon OOBB
		return a->IntersectsBoneCapsulesHierarchical(b->GetOOBB());
	}

	// OOBB vs BCapsule
	if ( a->GetType() == EColliderType::OOBB &&
		b->GetType() == EColliderType::BCapsule )
	{
		// world static vs body
		if ( layerA == kCollisionLayerWorldStatic )
			return a->IntersectsCapsuleHierarchical(b->GetBCapsule());

		// weapon OOBB vs body
		return b->IntersectsBoneCapsulesHierarchical(a->GetOOBB());
	}

	return false;
}

bool CCollisionSystem::HasCollisionWithWorldStatic(const CColliderComponent* subject) const
{
	if ( !subject )
		return false;

	for ( CColliderComponent* other : mColliders )
	{
		if ( !other ) continue;
		if ( other == subject ) continue;

		if ( other->GetLayer() != kCollisionLayerWorldStatic )
			continue;

		if ( IsPairIntersecting(subject, other) )
			return true;
	}

	return false;
}

void CCollisionSystem::HandlePair(CColliderComponent* a, CColliderComponent* b)
{
	if ( !a || !b )
		return;

	const bool isHit = IsPairIntersecting(a, b);
	if ( !isHit )
		return;

	if ( a->IsTrigger() || b->IsTrigger() )
	{
		return;
	}

}

bool CCollisionSystem::IsVisible(const BoundingFrustum& frustum, const CColliderComponent* collider)
{
	if ( collider->GetType() == EColliderType::BCapsule )
	{
		return collider->GetBCapsule().Intersects(frustum);
	}
	else if ( collider->GetType() == EColliderType::OOBB )
	{
		return collider->GetOOBB().Intersects(frustum);
	}
  return false;
}

void CCollisionSystem::OnUpdate()
{
    // 전수검사: i<j
    const size_t n = mColliders.size();
    for (size_t i = 0; i < n; ++i)
    {
        auto* a = mColliders[i];
        if (!a) continue;

		/*if ( !IsVisible(mCameraCollider, a) )
			a->DisabledRender();
			continue;*/

        for (size_t j = i + 1; j < n; ++j)
        {
            auto* b = mColliders[j];
            if (!b) continue;

            if (!PassFilter(a, b))
                continue;

			

            HandlePair(a, b);
        }
    }
}