//-----------------------------------------------------------------------------
// File: GameSceneBillboardCommon.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameSceneBillboardCommon.h"

namespace GameSceneBillboardCommon
{
	float DistanceSqXZ(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;

		return dx * dx + dz * dz;
	}

	float DistanceSq3(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		const float dx = a.x - b.x;
		const float dy = a.y - b.y;
		const float dz = a.z - b.z;

		return dx * dx + dy * dy + dz * dz;
	}

	void StoreCylindricalBillboardWorldRows(ItemBillboardInstanceVertex& dst, const XMFLOAT3& basePosition, float yOffset, float width, float height, const XMFLOAT3& targetPosition, UINT materialId)
	{
		const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		XMVECTOR center = XMLoadFloat3(&basePosition);
		center = XMVectorAdd(center, XMVectorSet(0.0f, yOffset, 0.0f, 0.0f));

		XMVECTOR target = XMLoadFloat3(&targetPosition);

		XMVECTOR forward = XMVectorSubtract(target, center);
		forward = XMVectorSetY(forward, 0.0f);

		if ( XMVectorGetX(XMVector3LengthSq(forward)) <= 1.0e-6f )
			forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		else
			forward = XMVector3Normalize(forward);

		XMVECTOR right = XMVector3Cross(up, forward);

		if ( XMVectorGetX(XMVector3LengthSq(right)) <= 1.0e-6f )
			right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		else
			right = XMVector3Normalize(right);

		const XMVECTOR scaledRight = XMVectorScale(right, width);
		const XMVECTOR scaledUp = XMVectorScale(up, height);

		XMFLOAT3 r{};
		XMFLOAT3 u{};
		XMFLOAT3 f{};
		XMFLOAT3 c{};

		XMStoreFloat3(&r, scaledRight);
		XMStoreFloat3(&u, scaledUp);
		XMStoreFloat3(&f, forward);
		XMStoreFloat3(&c, center);

		dst.world0 = XMFLOAT4(r.x, r.y, r.z, 0.0f);
		dst.world1 = XMFLOAT4(u.x, u.y, u.z, 0.0f);
		dst.world2 = XMFLOAT4(f.x, f.y, f.z, 0.0f);
		dst.world3 = XMFLOAT4(c.x, c.y, c.z, 1.0f);

		dst.materialId = materialId;
		dst.pad[0] = 0;
		dst.pad[1] = 0;
		dst.pad[2] = 0;
	}

	void StoreXZPlaneItemBillboardWorldRows(ItemBillboardInstanceVertex& dst, const XMFLOAT3& center, float yOffset, float width, float depth, UINT materialId)
	{
		dst.world0 = XMFLOAT4(width, 0.0f, 0.0f, 0.0f);
		dst.world1 = XMFLOAT4(0.0f, 0.0f, depth, 0.0f);
		dst.world2 = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
		dst.world3 = XMFLOAT4(center.x, center.y + yOffset, center.z, 1.0f);

		dst.materialId = materialId;
		dst.pad[0] = 0;
		dst.pad[1] = 0;
		dst.pad[2] = 0;
	}

	void StoreMuzzleFlashWorldRows(MuzzleFlashInstanceVertex& dst, const XMFLOAT3& basePosition, float width, float height, const XMFLOAT3& targetPosition)
	{
		const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		XMVECTOR center = XMLoadFloat3(&basePosition);
		XMVECTOR target = XMLoadFloat3(&targetPosition);

		XMVECTOR forward = XMVectorSubtract(target, center);
		forward = XMVectorSetY(forward, 0.0f);

		if ( XMVectorGetX(XMVector3LengthSq(forward)) <= 1.0e-6f )
			forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		else
			forward = XMVector3Normalize(forward);

		XMVECTOR right = XMVector3Cross(up, forward);

		if ( XMVectorGetX(XMVector3LengthSq(right)) <= 1.0e-6f )
			right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		else
			right = XMVector3Normalize(right);

		const XMVECTOR scaledRight = XMVectorScale(right, width);
		const XMVECTOR scaledUp = XMVectorScale(up, height);

		XMFLOAT3 r{};
		XMFLOAT3 u{};
		XMFLOAT3 f{};
		XMFLOAT3 c{};

		XMStoreFloat3(&r, scaledRight);
		XMStoreFloat3(&u, scaledUp);
		XMStoreFloat3(&f, forward);
		XMStoreFloat3(&c, center);

		dst.world0 = XMFLOAT4(r.x, r.y, r.z, 0.0f);
		dst.world1 = XMFLOAT4(u.x, u.y, u.z, 0.0f);
		dst.world2 = XMFLOAT4(f.x, f.y, f.z, 0.0f);
		dst.world3 = XMFLOAT4(c.x, c.y, c.z, 1.0f);
	}

	void StoreOrientedMuzzleFlashWorldRows(MuzzleFlashInstanceVertex& dst, const XMFLOAT3& centerPosition, const XMFLOAT3& rightAxis, const XMFLOAT3& upAxis, const XMFLOAT3& forwardAxis, float width, float height)
	{
		XMVECTOR right = XMLoadFloat3(&rightAxis);
		XMVECTOR up = XMLoadFloat3(&upAxis);
		XMVECTOR forward = XMLoadFloat3(&forwardAxis);

		if ( XMVectorGetX(XMVector3LengthSq(right)) <= 1.0e-8f )
			right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		else
			right = XMVector3Normalize(right);

		if ( XMVectorGetX(XMVector3LengthSq(up)) <= 1.0e-8f )
			up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		else
			up = XMVector3Normalize(up);

		if ( XMVectorGetX(XMVector3LengthSq(forward)) <= 1.0e-8f )
		{
			forward = XMVector3Cross(right, up);

			if ( XMVectorGetX(XMVector3LengthSq(forward)) <= 1.0e-8f )
				forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			else
				forward = XMVector3Normalize(forward);
		}
		else
		{
			forward = XMVector3Normalize(forward);
		}

		const XMVECTOR scaledRight = XMVectorScale(right, width);
		const XMVECTOR scaledUp = XMVectorScale(up, height);

		XMFLOAT3 r{};
		XMFLOAT3 u{};
		XMFLOAT3 f{};
		XMFLOAT3 c{};

		XMStoreFloat3(&r, scaledRight);
		XMStoreFloat3(&u, scaledUp);
		XMStoreFloat3(&f, forward);
		XMStoreFloat3(&c, XMLoadFloat3(&centerPosition));

		dst.world0 = XMFLOAT4(r.x, r.y, r.z, 0.0f);
		dst.world1 = XMFLOAT4(u.x, u.y, u.z, 0.0f);
		dst.world2 = XMFLOAT4(f.x, f.y, f.z, 0.0f);
		dst.world3 = XMFLOAT4(c.x, c.y, c.z, 1.0f);
	}
}