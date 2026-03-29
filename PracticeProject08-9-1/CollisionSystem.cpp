#include "stdafx.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"
#include "Object.h"

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

void CCollisionSystem::HandlePair(CColliderComponent* a, CColliderComponent* b)
{
    if (!a || !b)
        return;

    bool isHit = false;

    // 타입별 narrow phase
    if (a->GetType() == EColliderType::BCapsule && b->GetType() == EColliderType::BCapsule)
    {
        isHit = a->GetBCapsule().Intersects(b->GetBCapsule());
    }
    else if (a->GetType() == EColliderType::BCapsule && b->GetType() == EColliderType::OOBB)
    {
        isHit = a->GetBCapsule().Intersects(b->GetOOBB());
    }
    else if (a->GetType() == EColliderType::OOBB && b->GetType() == EColliderType::BCapsule)
    {
        isHit = b->GetBCapsule().Intersects(a->GetOOBB());
    }

    if (!isHit)
        return;

    if (a->IsTrigger() || b->IsTrigger())
    {
        // Trigger 이벤트 처리
        return;
    }

    auto* ownerA = a->GetOwner();
    if (!ownerA)
        return;

    auto* transformA = ownerA->GetComponent<CTransformComponent>();
    if (!transformA)
        return;

    const float pushBackDistance = 0.1f;

    XMFLOAT3 forward = transformA->direction;
    XMFLOAT3 pos = transformA->position;

    pos.x -= forward.x * pushBackDistance;
    pos.y -= forward.y * pushBackDistance;
    pos.z -= forward.z * pushBackDistance;

    transformA->Translate(pos);
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