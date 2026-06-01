//-----------------------------------------------------------------------------
// File: TerrainComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "TerrainComponent.h"
#include "Component.h"
#include "Object.h"
#include "TerrainData.h"

TerrainComponent::TerrainComponent(CGameObject* owner)
	: CComponentT<TerrainComponent>(owner)
{
	mTransform = owner->GetComponent<CTransformComponent>();
	assert(mTransform && "CColliderComponent requires CTransformComponent");
}

void TerrainComponent::SetTerrainData(std::shared_ptr<TerrainData> terrainData)
{
	m_terrainData = terrainData;
}

float TerrainComponent::GetHeightWorld(float worldX, float worldZ) const
{
	if (!m_terrainData)
		return 0.0f;

	XMFLOAT3 terrainPos = XMFLOAT3(0.0f, 0.0f, 0.0f);

	if (mTransform)
	{
		terrainPos = mTransform->position;
	}

	float localX = worldX - terrainPos.x;
	float localZ = worldZ - terrainPos.z;

	float localHeight = m_terrainData->GetHeight(localX, localZ);

	return terrainPos.y + localHeight;
}

XMFLOAT3 TerrainComponent::GetNormalWorld(float worldX, float worldZ) const
{
	if (!m_terrainData)
		return XMFLOAT3(0.0f, 1.0f, 0.0f);

	XMFLOAT3 terrainPos = XMFLOAT3(0.0f, 0.0f, 0.0f);

	if (mTransform)
	{
		terrainPos = mTransform->position;
	}

	float localX = worldX - terrainPos.x;
	float localZ = worldZ - terrainPos.z;

	return m_terrainData->GetNormal(localX, localZ);
}

float TerrainComponent::GetWidth() const
{
	return m_terrainData ? m_terrainData->GetWorldWidth() : 0.0f;
}

float TerrainComponent::GetLength() const
{
	return m_terrainData ? m_terrainData->GetWorldLength() : 0.0f;
}