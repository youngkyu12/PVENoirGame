#include "stdafx.h"
#include "TerrainAttachComponent.h"

#include "Object.h"
#include "TerrainData.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	float RoundToNearestTileCenter(float value, float tileSize)
	{
		return std::round(value / tileSize) * tileSize;
	}

	float RoundToNearestZTileCenter(float value, float tileSize)
	{
		return std::round((value - tileSize) / tileSize) * tileSize + tileSize;
	}
}

CTerrainAttachComponent::CTerrainAttachComponent(CGameObject* owner)
	: CComponentT<CTerrainAttachComponent>(owner)
{
}

CTerrainAttachComponent::CTerrainAttachComponent(
	CGameObject* owner,
	std::shared_ptr<TerrainData> terrainData)
	: CComponentT<CTerrainAttachComponent>(owner),
	m_terrainData(std::move(terrainData))
{
}

void CTerrainAttachComponent::SetTerrainData(std::shared_ptr<TerrainData> terrainData)
{
	m_terrainData = std::move(terrainData);
}

void CTerrainAttachComponent::SetHeightOffset(float offset)
{
	m_heightOffset = offset;
	m_captureInitialOffset = false;
}

void CTerrainAttachComponent::OnCreate(
	ID3D12Device* /*dev*/,
	ID3D12GraphicsCommandList* /*cmd*/)
{
	if (m_captureInitialOffset && GetOwner())
	{
		m_heightOffset = GetOwner()->GetPosition().y;
		m_captureInitialOffset = false;
	}

	SnapToTerrain();
}

void CTerrainAttachComponent::OnLateUpdate(float /*dt*/)
{
	SnapToTerrain();
}

void CTerrainAttachComponent::SnapToTerrain()
{
	CGameObject* owner = GetOwner();
	if (!owner || !m_terrainData)
		return;

	XMFLOAT3 pos = owner->GetPosition();
	const XMFLOAT3 terrainScale = m_terrainData->GetScale();
	const float terrainSize =
		static_cast<float>(m_terrainData->GetWidthCount() - 1) * terrainScale.x;
	if (terrainSize <= 0.0f)
		return;

	const float halfTerrainSize = terrainSize * 0.5f;
	const float centerX = RoundToNearestTileCenter(pos.x, terrainSize);
	const float centerZ = RoundToNearestZTileCenter(pos.z, terrainSize);
	const float originX = centerX - halfTerrainSize;
	const float originZ = centerZ - halfTerrainSize;

	const float maxLocalX =
		static_cast<float>(m_terrainData->GetWidthCount() - 2) * terrainScale.x;
	const float maxLocalZ =
		static_cast<float>(m_terrainData->GetLengthCount() - 2) * terrainScale.z;
	const float localX = std::clamp(pos.x - originX, 0.0f, maxLocalX);
	const float localZ = std::clamp(pos.z - originZ, 0.0f, maxLocalZ);
	const float terrainHeight = m_terrainData->GetHeight(localX, localZ);
	pos.y = terrainHeight + m_heightOffset;
	owner->SetPosition(pos);
}
