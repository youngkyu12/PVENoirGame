//-----------------------------------------------------------------------------
// File: TerrainComponent.h
//-----------------------------------------------------------------------------
#pragma once

#include "Component.h"

class TerrainData;
class CTransformComponent;

class TerrainComponent final : public CComponentT<TerrainComponent>
{
public:
	TerrainComponent() = default;
	explicit TerrainComponent(CGameObject* owner);
	virtual ~TerrainComponent() = default;

public:
	void SetTerrainData(std::shared_ptr<TerrainData> terrainData);

	float GetHeightWorld(float worldX, float worldZ) const;
	XMFLOAT3 GetNormalWorld(float worldX, float worldZ) const;
	float GetWidth() const;
	float GetLength() const;

private:
	CTransformComponent* mTransform = nullptr;
	std::shared_ptr<TerrainData> m_terrainData;
};