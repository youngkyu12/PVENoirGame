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
		kCollisionLayerCharacter = 0,
		kCollisionLayerWorldStatic = 1,
		kCollisionLayerLocalPlayer = 2
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

	if ( a->GetType() == EColliderType::BCapsule && b->GetType() == EColliderType::BCapsule )
	{
		return a->GetBCapsule().Intersects(b->GetBCapsule());
	}
	else if ( a->GetType() == EColliderType::BCapsule && b->GetType() == EColliderType::OOBB )
	{
		return b->IntersectsCapsuleHierarchical(a->GetBCapsule());
	}
	else if ( a->GetType() == EColliderType::OOBB && b->GetType() == EColliderType::BCapsule )
	{
		return a->IntersectsCapsuleHierarchical(b->GetBCapsule());
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

	bool isHit = IsPairIntersecting(a, b);
	if ( !isHit )
		return;

	CColliderComponent* pushedCollider = nullptr;

	if ( a->GetType() == EColliderType::BCapsule && b->GetType() == EColliderType::BCapsule )
	{
		pushedCollider = a;
	}
	else if ( a->GetType() == EColliderType::BCapsule && b->GetType() == EColliderType::OOBB )
	{
		pushedCollider = a;
	}
	else if ( a->GetType() == EColliderType::OOBB && b->GetType() == EColliderType::BCapsule )
	{
		pushedCollider = b;
	}

	//DebugPrintCollision(a, b);

	if ( a->IsTrigger() || b->IsTrigger() )
	{
		// Trigger 이벤트 처리
		return;
	}

	const bool isCapsuleVsOOBB =
		( a->GetType() == EColliderType::BCapsule && b->GetType() == EColliderType::OOBB ) ||
		( a->GetType() == EColliderType::OOBB && b->GetType() == EColliderType::BCapsule );

	if ( isCapsuleVsOOBB )
		return;

	if ( !pushedCollider )
		return;

	auto* owner = pushedCollider->GetOwner();
	if ( !owner )
		return;

	auto* transform = owner->GetComponent<CTransformComponent>();
	if ( !transform )
		return;

	const float pushBackDistance = 0.1f;

	XMFLOAT3 forward = transform->direction;
	XMVECTOR fwdV = XMLoadFloat3(&forward);

	if ( XMVectorGetX(XMVector3LengthSq(fwdV)) < 1e-8f )
		forward = XMFLOAT3(0.0f, 0.0f, 1.0f);

	XMFLOAT3 pos = transform->position;

	pos.x -= forward.x * pushBackDistance;
	pos.y -= forward.y * pushBackDistance;
	pos.z -= forward.z * pushBackDistance;

	transform->Translate(pos);
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

	/*for ( size_t i = 0; i < n; ++i )
	{
		auto* a = mColliders[i];

		if ( !a ) 
			continue;

		if ( !IsVisible(mCameraCollider, a) )
		{
			a->DisabledRender();
			continue;
		}
	}*/

    for (size_t i = 0; i < n; ++i)
    {
        auto* a = mColliders[i];
		if ( !a->IsRender() ) continue;
		else if ( !a )continue;

        for (size_t j = i + 1; j < n; ++j)
        {
            auto* b = mColliders[j];

            if (!b) 
				continue;

            if (!PassFilter(a, b))
                continue;

            HandlePair(a, b);
        }
    }
}