//-----------------------------------------------------------------------------
// File: ColliderComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "ColliderComponent.h"
#include "Object.h" // CGameObject

CColliderComponent::CColliderComponent(CGameObject* owner)
    : CComponentT<CColliderComponent>(owner)
{
}

void CColliderComponent::OnCreate(ID3D12Device*, ID3D12GraphicsCommandList*)
{
    mTransform = GetOwner()->GetComponent<CTransformComponent>();
    assert(mTransform && "CColliderComponent requires CTransformComponent");

    mModel = GetOwner()->GetComponent<CModelComponent>();
    assert(mModel && "CColliderComponent requires CModelComponent");

    const vector<shared_ptr<CMesh>>& meshes = mModel->GetMeshes();

    if (meshes.empty())
        return;

    switch (mColliderType)
    {
    case EColliderType::AABB:
        XMFLOAT3 objMin(FLT_MAX, FLT_MAX, FLT_MAX);
        XMFLOAT3 objMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (const shared_ptr<CMesh>& mesh : meshes)
        {
            if (!mesh) continue;

            objMin.x = min(objMin.x, mesh->GetMeshMin().x);
            objMin.y = min(objMin.y, mesh->GetMeshMin().y);
            objMin.z = min(objMin.z, mesh->GetMeshMin().z);

            objMax.x = max(objMax.x, mesh->GetMeshMax().x);
            objMax.y = max(objMax.y, mesh->GetMeshMax().y);
            objMax.z = max(objMax.z, mesh->GetMeshMax().z);

            SetAABB(objMin, objMax);
        }
    case EColliderType::OOBB:
        break;
    case EColliderType::BSphere:
        break;
    default:
        break;
    }
    
    UpdateWorldBounds();
}

void CColliderComponent::OnUpdate(float)
{
    // MVP: 매 프레임 갱신
    UpdateWorldBounds();
}

void CColliderComponent::SetAABB(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
    AABB.Center = XMFLOAT3(
        (Min.x + Max.x) * 0.5f,
        (Min.y + Max.y) * 0.5f,
        (Min.z + Max.z) * 0.5f);

    AABB.Extents = XMFLOAT3(
        (Max.x - Min.x) * 0.5f,
        (Max.y - Min.y) * 0.5f,
        (Max.z - Min.z) * 0.5f);
}

void CColliderComponent::UpdateWorldBounds()
{
    if (!mTransform) return;

    // 1) 월드 행렬 준비 (엔진 함수에 맞게 바꿔)
    // XMMATRIX W = mTransform->GetWorldMatrixXM();
    XMMATRIX W =
        XMMatrixScaling(mTransform->scale.x, mTransform->scale.y, mTransform->scale.z) *
        XMMatrixRotationRollPitchYaw(mTransform->rotation.x, mTransform->rotation.y, mTransform->rotation.z) *
        XMMatrixTranslation(mTransform->position.x, mTransform->position.y, mTransform->position.z);

    switch (mColliderType)
    {
    case EColliderType::AABB:
    {
        // 로컬 AABB -> 월드 AABB (회전 포함하면 자동으로 "월드 AABB"로 감싸줌)
        BoundingBox worldBB;
        AABB.Transform(worldBB, W);

        // 월드 캐시 업데이트
        Center = worldBB.Center;

        const XMFLOAT3& e = worldBB.Extents;
        Radius = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z);

        break;
    }

    case EColliderType::OOBB:
    {
        break;
    }

    case EColliderType::BSphere:
    {
        break;
    }

    default:
        // None이면 캐시만 리셋하거나 무시
        break;
    }
}

bool CColliderComponent::Intersects(const CColliderComponent& other) const
{
    // AABB only
    if (mColliderType != EColliderType::AABB || other.mColliderType != EColliderType::AABB)
        return false;

    // Both colliders must have transforms
    if (!mTransform || !other.mTransform)
        return false;

    // Build world matrices (match your transform representation: position/rotation/scale)
    const XMMATRIX W0 =
        XMMatrixScaling(mTransform->scale.x, mTransform->scale.y, mTransform->scale.z) *
        XMMatrixRotationRollPitchYaw(mTransform->rotation.x, mTransform->rotation.y, mTransform->rotation.z) *
        XMMatrixTranslation(mTransform->position.x, mTransform->position.y, mTransform->position.z);

    const XMMATRIX W1 =
        XMMatrixScaling(other.mTransform->scale.x, other.mTransform->scale.y, other.mTransform->scale.z) *
        XMMatrixRotationRollPitchYaw(other.mTransform->rotation.x, other.mTransform->rotation.y, other.mTransform->rotation.z) *
        XMMatrixTranslation(other.mTransform->position.x, other.mTransform->position.y, other.mTransform->position.z);

    // Transform local AABBs to world AABBs
    BoundingBox aWorld, bWorld;
    AABB.Transform(aWorld, W0);
    other.AABB.Transform(bWorld, W1);

    // AABB vs AABB test (DirectXCollision)
    return aWorld.Intersects(bWorld);
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