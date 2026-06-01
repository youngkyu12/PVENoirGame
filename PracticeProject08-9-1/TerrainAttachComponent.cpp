//-----------------------------------------------------------------------------
// File: TerrainAttachComponent.cpp
//-----------------------------------------------------------------------------
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
	if ( !owner || !m_terrainData )
		return;

	XMFLOAT3 pos = owner->GetPosition();
	const XMFLOAT3 terrainPos = m_terrainData->GetWorldPosition();

	const float localX = pos.x - terrainPos.x;
	const float localZ = pos.z - terrainPos.z;

	const float fHeight = terrainPos.y + m_terrainData->GetHeight(localX, localZ);

	if ( pos.y < fHeight )
	{
		pos.y = fHeight;
		owner->SetPosition(pos);
	}
}
