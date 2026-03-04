#include "stdafx.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"
#include "Object.h"

CCollisionSystem::CCollisionSystem(CGameObject* owner)
	: CComponentT<CCollisionSystem>(owner)
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
    // 실제 겹침 판정 (네 collider가 이미 Intersects 제공)
    const bool overlapping = a->Intersects(*b);

    const bool was = a->WasOverlapping(b); // a쪽 set만 봐도 됨(대칭으로 관리할 거라서)

    if (overlapping && !was)
    {
        // Enter
        a->MarkOverlapping(b);
        b->MarkOverlapping(a);

        if (a->IsTrigger() || b->IsTrigger())
        {
            a->OnTriggerEnter(b);
            b->OnTriggerEnter(a);
        }
        else
        {
            a->OnCollisionEnter(b);
            b->OnCollisionEnter(a);
        }
    }
    else if (!overlapping && was)
    {
        // Exit
        a->UnmarkOverlapping(b);
        b->UnmarkOverlapping(a);

        if (a->IsTrigger() || b->IsTrigger())
        {
            a->OnTriggerExit(b);
            b->OnTriggerExit(a);
        }
        else
        {
            a->OnCollisionExit(b);
            b->OnCollisionExit(a);
        }
    }

    // overlapping && was : 유지 상태 -> 필요하면 OnStay 같은 이벤트 추가
}

void CCollisionSystem::OnUpdate(float /*dt*/)
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