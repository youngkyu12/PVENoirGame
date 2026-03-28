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

void CCollisionSystem::HandlePair(CColliderComponent* a, CColliderComponent* b)
{
	if ( !a || !b )
		return;

	bool isHit = false;
	CColliderComponent* pushedCollider = nullptr;

	// 타입별 narrow phase
	if ( a->GetType() == EColliderType::BCapsule && b->GetType() == EColliderType::BCapsule )
	{
		isHit = a->GetBCapsule().Intersects(b->GetBCapsule());
		pushedCollider = a;
	}
	else if ( a->GetType() == EColliderType::BCapsule && b->GetType() == EColliderType::OOBB )
	{
		isHit = b->IntersectsCapsuleHierarchical(a->GetBCapsule());
		pushedCollider = a;
	}
	else if ( a->GetType() == EColliderType::OOBB && b->GetType() == EColliderType::BCapsule )
	{
		isHit = a->IntersectsCapsuleHierarchical(b->GetBCapsule());
		pushedCollider = b;
	}

	if ( !isHit )
		return;

	DebugPrintCollision(a, b);

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

void CCollisionSystem::OnUpdate()
{
    // 전수검사: i<j
    const size_t n = mColliders.size();
    for (size_t i = 0; i < n; ++i)
    {
        auto* a = mColliders[i];
        if (!a) continue;

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