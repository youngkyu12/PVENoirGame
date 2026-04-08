//-----------------------------------------------------------------------------
// File: ColliderComponent.cpp
//-----------------------------------------------------------------------------
#include "pch.h"
#include "ColliderComponent.h"
#include "ServerObject.h"

namespace
{
	inline XMFLOAT3 ToFloat3(const GameMath::Vec3& v)
	{
		return XMFLOAT3(v.x, v.y, v.z);
	}
}

CColliderComponent::CColliderComponent(OwnerT* owner, EColliderType type)
	: CComponentT<CColliderComponent>(owner)
	, mColliderType(type)
{
}

void CColliderComponent::OnCreate()
{
	if (auto* owner = GetOwner())
		mTransform = owner->GetComponent<CCommonTransformComponent>();

	UpdateWorldBounds();
}

void CColliderComponent::OnUpdate(float)
{
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

	LocalOOBB.Orientation = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
}

void CColliderComponent::SetOOBB(const BoundingOrientedBox& localOOBB)
{
	LocalOOBB = localOOBB;
}

void CColliderComponent::SetSubOOBBs(const std::vector<BoundingOrientedBox>& localSubOOBBs)
{
	LocalSubOOBBs = localSubOOBBs;
}

void CColliderComponent::ClearSubOOBBs()
{
	LocalSubOOBBs.clear();
	WorldSubOOBBs.clear();
}

void CColliderComponent::SetBSphere(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
	LocalBSphere.Center = XMFLOAT3(
		(Min.x + Max.x) * 0.5f,
		(Min.y + Max.y) * 0.5f,
		(Min.z + Max.z) * 0.5f);

	const XMFLOAT3 extents(
		(Max.x - Min.x) * 0.5f,
		(Max.y - Min.y) * 0.5f,
		(Max.z - Min.z) * 0.5f);

	LocalBSphere.Radius = sqrtf(extents.x * extents.x + extents.y * extents.y + extents.z * extents.z);
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

	if (dx >= dy && dx >= dz)
	{
		LocalBCapsule.Height = dx;
		LocalBCapsule.Radius = (std::max)(dy, dz) * 0.5f;
		const float halfSegment = (std::max)(0.0f, dx * 0.5f - LocalBCapsule.Radius);

		LocalBCapsule.p0 = XMFLOAT3(LocalBCapsule.Center.x - halfSegment, LocalBCapsule.Center.y, LocalBCapsule.Center.z);
		LocalBCapsule.p1 = XMFLOAT3(LocalBCapsule.Center.x + halfSegment, LocalBCapsule.Center.y, LocalBCapsule.Center.z);
		LocalBCapsule.Direction = EDirection::X;
	}
	else if (dy >= dx && dy >= dz)
	{
		LocalBCapsule.Height = dy;
		LocalBCapsule.Radius = (std::max)(dx, dz) * 0.5f;
		const float halfSegment = (std::max)(0.0f, dy * 0.5f - LocalBCapsule.Radius);

		LocalBCapsule.p0 = XMFLOAT3(LocalBCapsule.Center.x, LocalBCapsule.Center.y - halfSegment, LocalBCapsule.Center.z);
		LocalBCapsule.p1 = XMFLOAT3(LocalBCapsule.Center.x, LocalBCapsule.Center.y + halfSegment, LocalBCapsule.Center.z);
		LocalBCapsule.Direction = EDirection::Y;
	}
	else
	{
		LocalBCapsule.Height = dz;
		LocalBCapsule.Radius = (std::max)(dx, dy) * 0.5f;
		const float halfSegment = (std::max)(0.0f, dz * 0.5f - LocalBCapsule.Radius);

		LocalBCapsule.p0 = XMFLOAT3(LocalBCapsule.Center.x, LocalBCapsule.Center.y, LocalBCapsule.Center.z - halfSegment);
		LocalBCapsule.p1 = XMFLOAT3(LocalBCapsule.Center.x, LocalBCapsule.Center.y, LocalBCapsule.Center.z + halfSegment);
		LocalBCapsule.Direction = EDirection::Z;
	}
}

void CColliderComponent::SetSubBCapsule(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
	BoundingCapsule capsule;
	capsule.Center = XMFLOAT3(
		(Min.x + Max.x) * 0.5f,
		(Min.y + Max.y) * 0.5f,
		(Min.z + Max.z) * 0.5f);

	const float dx = Max.x - Min.x;
	const float dy = Max.y - Min.y;
	const float dz = Max.z - Min.z;

	if (dx >= dy && dx >= dz)
	{
		capsule.Height = dx;
		capsule.Radius = (std::max)(dy, dz) * 0.5f;
		const float halfSegment = (std::max)(0.0f, dx * 0.5f - capsule.Radius);
		capsule.p0 = XMFLOAT3(capsule.Center.x - halfSegment, capsule.Center.y, capsule.Center.z);
		capsule.p1 = XMFLOAT3(capsule.Center.x + halfSegment, capsule.Center.y, capsule.Center.z);
		capsule.Direction = EDirection::X;
	}
	else if (dy >= dx && dy >= dz)
	{
		capsule.Height = dy;
		capsule.Radius = (std::max)(dx, dz) * 0.5f;
		const float halfSegment = (std::max)(0.0f, dy * 0.5f - capsule.Radius);
		capsule.p0 = XMFLOAT3(capsule.Center.x, capsule.Center.y - halfSegment, capsule.Center.z);
		capsule.p1 = XMFLOAT3(capsule.Center.x, capsule.Center.y + halfSegment, capsule.Center.z);
		capsule.Direction = EDirection::Y;
	}
	else
	{
		capsule.Height = dz;
		capsule.Radius = (std::max)(dx, dy) * 0.5f;
		const float halfSegment = (std::max)(0.0f, dz * 0.5f - capsule.Radius);
		capsule.p0 = XMFLOAT3(capsule.Center.x, capsule.Center.y, capsule.Center.z - halfSegment);
		capsule.p1 = XMFLOAT3(capsule.Center.x, capsule.Center.y, capsule.Center.z + halfSegment);
		capsule.Direction = EDirection::Z;
	}

	LocalSubBCapsules.push_back(capsule);
}

void CColliderComponent::UpdateWorldBounds()
{
	if (!mTransform)
		return;

	const XMMATRIX S = XMMatrixScaling(mTransform->scale.x, mTransform->scale.y, mTransform->scale.z);
	const XMMATRIX R = XMMatrixRotationY(XMConvertToRadians(mTransform->yaw));
	const XMMATRIX T = XMMatrixTranslation(mTransform->position.x, mTransform->position.y, mTransform->position.z);
	const XMMATRIX W = S * R * T;

	switch (mColliderType)
	{
	case EColliderType::AABB:
		LocalAABB.Transform(WorldAABB, W);
		break;
	case EColliderType::OOBB:
		LocalOOBB.Transform(WorldOOBB, W);
      WorldSubOOBBs.resize(LocalSubOOBBs.size());
		for (size_t i = 0; i < LocalSubOOBBs.size(); ++i)
			LocalSubOOBBs[i].Transform(WorldSubOOBBs[i], W);
		break;
	case EColliderType::BSphere:
		LocalBSphere.Transform(WorldBSphere, W);
		break;
	case EColliderType::BCapsule:
		LocalBCapsule.Transform(WorldBCapsule, W);
		WorldSubBCapsules.resize(LocalSubBCapsules.size());
		for (size_t i = 0; i < LocalSubBCapsules.size(); ++i)
			LocalSubBCapsules[i].Transform(WorldSubBCapsules[i], W);
		break;
	default:
		break;
	}
}

bool CColliderComponent::IntersectsCapsuleHierarchical(const BoundingCapsule& capsule) const
{
	if (mColliderType != EColliderType::OOBB)
		return false;

	if (!capsule.Intersects(WorldOOBB))
		return false;

	if (WorldSubOOBBs.empty())
		return true;

	for (const BoundingOrientedBox& subBox : WorldSubOOBBs)
	{
		if (capsule.Intersects(subBox))
			return true;
	}

   return false;
}