//-----------------------------------------------------------------------------
// File: ColliderComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "ColliderComponent.h"
#include "Object.h" // CGameObject

CColliderComponent::CColliderComponent(CGameObject* owner, EColliderType Type)
    : CComponentT<CColliderComponent>(owner)
{
    mColliderType = Type;
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
    XMFLOAT3 objMin = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
    XMFLOAT3 objMax = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    switch (mColliderType)
    {
    case EColliderType::AABB:
        for (const shared_ptr<CMesh>& mesh : meshes)
        {
            if (!mesh) continue;

            for (const vector<SubMesh>::iterator::value_type& submesh : mesh->m_SubMeshes)
            {
                objMin.x = min(objMin.x, submesh.subMeshMin.x);
                objMin.y = min(objMin.y, submesh.subMeshMin.y);
                objMin.z = min(objMin.z, submesh.subMeshMin.z);

                objMax.x = max(objMax.x, submesh.subMeshMax.x);
                objMax.y = max(objMax.y, submesh.subMeshMax.y);
                objMax.z = max(objMax.z, submesh.subMeshMax.z);
            }
            SetAABB(objMin, objMax);
        }
        break;
    case EColliderType::OOBB:
        for (const shared_ptr<CMesh>& mesh : meshes)
        {
            if (!mesh) continue;

            for (const auto& submesh : mesh->m_SubMeshes)
            {
                objMin.x = min(objMin.x, submesh.subMeshMin.x);
                objMin.y = min(objMin.y, submesh.subMeshMin.y);
                objMin.z = min(objMin.z, submesh.subMeshMin.z);

                objMax.x = max(objMax.x, submesh.subMeshMax.x);
                objMax.y = max(objMax.y, submesh.subMeshMax.y);
                objMax.z = max(objMax.z, submesh.subMeshMax.z);
            }
            SetOOBB(objMin, objMax);
        }
        
        break;
    case EColliderType::BSphere:

        break;
    case EColliderType::BCapsule:
        for (const shared_ptr<CMesh>& mesh : meshes)
        {
            if (!mesh) continue;

            for (const auto& submesh : mesh->m_SubMeshes)
            {
                objMin.x = min(objMin.x, submesh.subMeshMin.x);
                objMin.y = min(objMin.y, submesh.subMeshMin.y);
                objMin.z = min(objMin.z, submesh.subMeshMin.z);

                objMax.x = max(objMax.x, submesh.subMeshMax.x);
                objMax.y = max(objMax.y, submesh.subMeshMax.y);
                objMax.z = max(objMax.z, submesh.subMeshMax.z);
                SetSubBCapsule(submesh.subMeshMin, submesh.subMeshMax);
            }
            SetBCapsule(objMin, objMax);
        }
        
        break;
    default:
        break;
    }
    
    UpdateWorldBounds();
}

void CColliderComponent::OnUpdate(float dt)
{
    // MVP: 매 프레임 갱신
    UpdateWorldBounds();
}

void CColliderComponent::SetAABB(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
    LocalAABB.Center = XMFLOAT3(
        (Min.x + Max.x) * 0.5f,
        (Min.y + Max.y) * 0.5f,
        (Min.z + Max.z) * 0.5f);

    LocalAABB.Extents = XMFLOAT3(
        (Max.x - Min.x) * 0.5f,
        (Max.y - Min.y) * 0.5f,
        (Max.z - Min.z) * 0.5f);
}

void CColliderComponent::SetOOBB(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
    LocalOOBB.Center = XMFLOAT3(
        (Min.x + Max.x) * 0.5f,
        (Min.y + Max.y) * 0.5f,
        (Min.z + Max.z) * 0.5f);

    LocalOOBB.Extents = XMFLOAT3(
        (Max.x - Min.x) * 0.5f,
        (Max.y - Min.y) * 0.5f,
        (Max.z - Min.z) * 0.5f);
}

void CColliderComponent::SetBSphere(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
    LocalBSphere.Center = XMFLOAT3(
        (Min.x + Max.x) * 0.5f,
        (Min.y + Max.y) * 0.5f,
        (Min.z + Max.z) * 0.5f);

    XMFLOAT3 Extents = XMFLOAT3(
        (Max.x - Min.x) * 0.5f,
        (Max.y - Min.y) * 0.5f,
        (Max.z - Min.z) * 0.5f);

    float Radius = sqrt(Extents.x * Extents.x + Extents.y * Extents.y + Extents.z * Extents.z);
    LocalBSphere.Radius = Radius;
}

void CColliderComponent::SetBCapsule(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
    LocalBCapsule.Center = XMFLOAT3(
        (Min.x + Max.x) * 0.5f,
        (Min.y + Max.y) * 0.5f,
        (Min.z + Max.z) * 0.5f);

    const float dx = Max.x - Min.x;
    const float dy = Max.y - Min.y;
    const float dz = Max.z - Min.z;

    if (dx >= dy && dx >= dz) {
        LocalBCapsule.Height = dx;
        LocalBCapsule.Radius = max(dy, dz) * 0.5f;

        const float halfSegment = max(0.0f, dx * 0.5f - LocalBCapsule.Radius);

        LocalBCapsule.p0 = XMFLOAT3(
            LocalBCapsule.Center.x - halfSegment,
            LocalBCapsule.Center.y,
            LocalBCapsule.Center.z);

        LocalBCapsule.p1 = XMFLOAT3(
            LocalBCapsule.Center.x + halfSegment,
            LocalBCapsule.Center.y,
            LocalBCapsule.Center.z);
        LocalBCapsule.Direction = EDirection::X;
    }
    else if (dy >= dx && dy >= dz) {
        LocalBCapsule.Height = dy;
        LocalBCapsule.Radius = max(dx, dz) * 0.5f;

        const float halfSegment = max(0.0f, dy * 0.5f - LocalBCapsule.Radius);

        LocalBCapsule.p0 = XMFLOAT3(
            LocalBCapsule.Center.x,
            LocalBCapsule.Center.y - halfSegment,
            LocalBCapsule.Center.z);

        LocalBCapsule.p1 = XMFLOAT3(
            LocalBCapsule.Center.x,
            LocalBCapsule.Center.y + halfSegment,
            LocalBCapsule.Center.z);
        LocalBCapsule.Direction = EDirection::Y;
    }
    else {
        LocalBCapsule.Height = dz;
        const float halfSegment = max(0.0f, dz * 0.5f - LocalBCapsule.Radius);

        LocalBCapsule.p0 = XMFLOAT3(
            LocalBCapsule.Center.x,
            LocalBCapsule.Center.y,
            LocalBCapsule.Center.z - halfSegment);

        LocalBCapsule.p1 = XMFLOAT3(
            LocalBCapsule.Center.x,
            LocalBCapsule.Center.y,
            LocalBCapsule.Center.z + halfSegment);
        LocalBCapsule.Direction = EDirection::Z;
    }
   
}

void CColliderComponent::SetSubBCapsule(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
    BoundingCapsule Capsule;
    Capsule.Center = XMFLOAT3(
        (Min.x + Max.x) * 0.5f,
        (Min.y + Max.y) * 0.5f,
        (Min.z + Max.z) * 0.5f);

    const float dx = Max.x - Min.x;
    const float dy = Max.y - Min.y;
    const float dz = Max.z - Min.z;

    if (dx >= dy && dx >= dz) {
        Capsule.Height = dx;
        Capsule.Radius = max(dy, dz) * 0.5f;

        const float halfSegment = max(0.0f, dx * 0.5f - Capsule.Radius);

        Capsule.p0 = XMFLOAT3(
            Capsule.Center.x - halfSegment,
            Capsule.Center.y,
            Capsule.Center.z);

        Capsule.p1 = XMFLOAT3(
            Capsule.Center.x + halfSegment,
            Capsule.Center.y,
            Capsule.Center.z);
        Capsule.Direction = EDirection::X;
    }
    else if (dy >= dx && dy >= dz) {
        Capsule.Height = dy;
        Capsule.Radius = max(dx, dz) * 0.5f;

        const float halfSegment = max(0.0f, dy * 0.5f - Capsule.Radius);

        Capsule.p0 = XMFLOAT3(
            Capsule.Center.x,
            Capsule.Center.y - halfSegment,
            Capsule.Center.z);

        Capsule.p1 = XMFLOAT3(
            Capsule.Center.x,
            Capsule.Center.y + halfSegment,
            Capsule.Center.z);
        Capsule.Direction = EDirection::Y;
    }
    else {
        Capsule.Height = dz;
        const float halfSegment = max(0.0f, dz * 0.5f - Capsule.Radius);

        Capsule.p0 = XMFLOAT3(
            Capsule.Center.x,
            Capsule.Center.y,
            Capsule.Center.z - halfSegment);

        Capsule.p1 = XMFLOAT3(
            Capsule.Center.x,
            Capsule.Center.y,
            Capsule.Center.z + halfSegment);
        Capsule.Direction = EDirection::Z;
    }

    LocalSubBCapsules.push_back(Capsule);
}

void CColliderComponent::UpdateWorldBounds()
{
    if (!mTransform) return;

    XMMATRIX S = XMMatrixScaling(mTransform->scale.x, mTransform->scale.y, mTransform->scale.z);
    XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&mTransform->rotation));
    XMMATRIX T = XMMatrixTranslation(mTransform->position.x, mTransform->position.y, mTransform->position.z);

    XMMATRIX W = S * R * T;

    switch (mColliderType)
    {
    case EColliderType::AABB:
    {
        LocalAABB.Transform(WorldAABB, W);
        break;
    }
    case EColliderType::OOBB:
    {
        LocalOOBB.Transform(WorldOOBB, W);
        break;
    }
    case EColliderType::BSphere:
    {
        LocalBSphere.Transform(WorldBSphere, W);
        break;
    }
    case EColliderType::BCapsule:
    {
        LocalBCapsule.Transform(WorldBCapsule, W);
        break;
    }
    default:
        // None이면 캐시만 리셋하거나 무시
        break;
    }
}

