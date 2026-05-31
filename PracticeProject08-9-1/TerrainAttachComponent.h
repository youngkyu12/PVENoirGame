#pragma once

#include "Component.h"

#include <memory>

class TerrainData;

class CTerrainAttachComponent final : public CComponentT<CTerrainAttachComponent>
{
public:
	explicit CTerrainAttachComponent(CGameObject* owner);
	CTerrainAttachComponent(CGameObject* owner, std::shared_ptr<TerrainData> terrainData);

	void SetTerrainData(std::shared_ptr<TerrainData> terrainData);
	void SetHeightOffset(float offset);
	float GetHeightOffset() const { return m_heightOffset; }

	void SnapToTerrain();

	void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;
	void OnLateUpdate(float dt) override;

private:
	std::shared_ptr<TerrainData> m_terrainData;
	float m_heightOffset = 0.0f;
	bool m_captureInitialOffset = true;
};
