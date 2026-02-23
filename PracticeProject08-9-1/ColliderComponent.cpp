//-----------------------------------------------------------------------------
// File: ColliderComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "ColliderComponent.h"
#include "Object.h" // CGameObject

static inline float ClampFloat(float v, float lo, float hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

static inline bool AABBOverlap(const AABB& a, const AABB& b)
{
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
        (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
        (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

static inline bool SphereSphere(const XMFLOAT3& ca, float ra, const XMFLOAT3& cb, float rb)
{
    const float dx = ca.x - cb.x;
    const float dy = ca.y - cb.y;
    const float dz = ca.z - cb.z;
    const float r = ra + rb;
    return (dx * dx + dy * dy + dz * dz) <= (r * r);
}

static inline bool SphereAABB(const XMFLOAT3& c, float r, const AABB& b)
{
    const float cx = ClampFloat(c.x, b.min.x, b.max.x);
    const float cy = ClampFloat(c.y, b.min.y, b.max.y);
    const float cz = ClampFloat(c.z, b.min.z, b.max.z);

    const float dx = c.x - cx;
    const float dy = c.y - cy;
    const float dz = c.z - cz;
    return (dx * dx + dy * dy + dz * dz) <= (r * r);
}

CColliderComponent::CColliderComponent(CGameObject* owner)
    : CComponentT<CColliderComponent>(owner)
{
}

void CColliderComponent::OnCreate(ID3D12Device*, ID3D12GraphicsCommandList*)
{
    mTransform = GetOwner()->GetComponent<CTransformComponent>();
    assert(mTransform && "CColliderComponent requires CTransformComponent");
    UpdateWorldBounds();
}

void CColliderComponent::OnUpdate(float)
{
    // MVP: 매 프레임 갱신
    UpdateWorldBounds();
}

void CColliderComponent::SetSphere(float radius, const XMFLOAT3& localCenter)
{
    mType = EColliderType::Sphere;
    mRadius = radius;
    mLocalCenter = localCenter;
    UpdateWorldBounds();
}

void CColliderComponent::SetAABB(const XMFLOAT3& halfExtents, const XMFLOAT3& localCenter)
{
    mType = EColliderType::AABB;
    mHalfExtents = halfExtents;
    mLocalCenter = localCenter;
    UpdateWorldBounds();
}

void CColliderComponent::UpdateWorldBounds()
{
    if (!mTransform) return;

    // 간단 버전: position + localCenter
    const XMFLOAT3 pos = mTransform->position;
    mWorldCenter = XMFLOAT3(pos.x + mLocalCenter.x, pos.y + mLocalCenter.y, pos.z + mLocalCenter.z);

    const float sx = mTransform->scale.x;
    const float sy = mTransform->scale.y;
    const float sz = mTransform->scale.z;
    const float smax = max(sx, max(sy, sz));

    if (mType == EColliderType::Sphere)
    {
        mWorldRadius = mRadius * smax;

        mWorldAABB.min = XMFLOAT3(mWorldCenter.x - mWorldRadius,
            mWorldCenter.y - mWorldRadius,
            mWorldCenter.z - mWorldRadius);
        mWorldAABB.max = XMFLOAT3(mWorldCenter.x + mWorldRadius,
            mWorldCenter.y + mWorldRadius,
            mWorldCenter.z + mWorldRadius);
    }
    else if (mType == EColliderType::AABB)
    {
        // 회전 없는 AABB: halfExtents에 scale 반영
        const XMFLOAT3 he(mHalfExtents.x * sx, mHalfExtents.y * sy, mHalfExtents.z * sz);

        mWorldAABB.min = XMFLOAT3(mWorldCenter.x - he.x,
            mWorldCenter.y - he.y,
            mWorldCenter.z - he.z);
        mWorldAABB.max = XMFLOAT3(mWorldCenter.x + he.x,
            mWorldCenter.y + he.y,
            mWorldCenter.z + he.z);

        // broadphase 참고용(선택)
        mWorldRadius = sqrtf(he.x * he.x + he.y * he.y + he.z * he.z);
    }
}

bool CColliderComponent::Intersects(const CColliderComponent& other) const
{
    // layer/mask 필터: (mask에 상대 layer bit가 켜져 있어야 충돌)
    if (((mMask & (1u << other.mLayer)) == 0) || ((other.mMask & (1u << mLayer)) == 0))
        return false;

    // broadphase
    if (!AABBOverlap(mWorldAABB, other.mWorldAABB))
        return false;

    // narrowphase
    if (mType == EColliderType::Sphere && other.mType == EColliderType::Sphere)
        return SphereSphere(mWorldCenter, mWorldRadius, other.mWorldCenter, other.mWorldRadius);

    if (mType == EColliderType::AABB && other.mType == EColliderType::AABB)
        return true; // 회전 없는 AABB면 broadphase로 충분

    if (mType == EColliderType::Sphere && other.mType == EColliderType::AABB)
        return SphereAABB(mWorldCenter, mWorldRadius, other.mWorldAABB);

    if (mType == EColliderType::AABB && other.mType == EColliderType::Sphere)
        return SphereAABB(other.mWorldCenter, other.mWorldRadius, mWorldAABB);

    return false;
}

bool CColliderComponent::WasOverlapping(CColliderComponent* other) const
{
    return mOverlaps.find(other) != mOverlaps.end();
}

void CColliderComponent::MarkOverlapping(CColliderComponent* other)
{
    mOverlaps.insert(other);
}

void CColliderComponent::UnmarkOverlapping(CColliderComponent* other)
{
    mOverlaps.erase(other);
}