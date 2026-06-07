//-----------------------------------------------------------------------------
// File: GameSceneBillboardCommon.h
//-----------------------------------------------------------------------------

#pragma once

#include "GameSceneBillboardTypes.h"

namespace GameSceneBillboardCommon
{
	float DistanceSqXZ(const XMFLOAT3& a, const XMFLOAT3& b);
	float DistanceSq3(const XMFLOAT3& a, const XMFLOAT3& b);

	void StoreCylindricalBillboardWorldRows(ItemBillboardInstanceVertex& dst, const XMFLOAT3& basePosition, float yOffset, float width, float height, const XMFLOAT3& targetPosition, UINT materialId);
	void StoreXZPlaneItemBillboardWorldRows(ItemBillboardInstanceVertex& dst, const XMFLOAT3& center, float yOffset, float width, float depth, UINT materialId);

	void StoreMuzzleFlashWorldRows(MuzzleFlashInstanceVertex& dst, const XMFLOAT3& basePosition, float width, float height, const XMFLOAT3& targetPosition);
	void StoreOrientedMuzzleFlashWorldRows(MuzzleFlashInstanceVertex& dst, const XMFLOAT3& centerPosition, const XMFLOAT3& rightAxis, const XMFLOAT3& upAxis, const XMFLOAT3& forwardAxis, float width, float height);
}