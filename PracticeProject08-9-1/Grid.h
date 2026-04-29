//-----------------------------------------------------------------------------
// File: Grid.h
//-----------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include <DirectXCollision.h>

class CGameObject;

class CSceneGrid
{
public:
	static constexpr int kGridMinX = -600;
	static constexpr int kGridMaxX = 600;
	static constexpr int kGridMinZ = -200;
	static constexpr int kGridMaxZ = 1000;

	static constexpr int kGridWidth = ( kGridMaxX - kGridMinX );
	static constexpr int kGridHeight = ( kGridMaxZ - kGridMinZ );
	static constexpr int kGridCellCount = ( kGridWidth * kGridHeight );

	static constexpr int kMegaGridCols = 3;
	static constexpr int kMegaGridRows = 3;
	static constexpr int kMegaGridCount = ( kMegaGridCols * kMegaGridRows );

	static constexpr int kMegaGridCellWidth = ( kGridWidth / kMegaGridCols );
	static constexpr int kMegaGridCellHeight = ( kGridHeight / kMegaGridRows );

	static constexpr int kDefaultMegaGridApproachWidth = 200;
	static constexpr int kDefaultMegaGridApproachHeight = 200;

	static constexpr int kTreeCullVillageCenterSizeCells = 200;
	static constexpr int kTreeCullVillageInnerBlockedSizeCells = 100;

	static constexpr float kTreeCullVillageHalfSize = 100.0f;
	static constexpr float kTreeCullVillageInnerBlockedHalfSize = 50.0f;

	static constexpr float kTreeCullGateHalfWidth = 4.0f;
	static constexpr float kTreeCullGateDepth = 17.0f;

	static constexpr int kTreeCullLooseGateHalfWidthCells = 24;
	static constexpr int kTreeCullLooseGateInsideDepthCells = 45;

	static constexpr int kTreeCullRaycastExtraGateHalfWidthCells = 8;
	static constexpr int kTreeCullRaycastExtraGateDepthCells = 8;

	struct MegaGridCell
	{
		bool hasPlayerApproached = false;
		bool isCleared = false;
		bool hasEventOccurred = false;

		int approachWidthCells = kDefaultMegaGridApproachWidth;
		int approachHeightCells = kDefaultMegaGridApproachHeight;
	};

	struct GridStaticCell
	{
		uint16_t buildingCount = 0;
		float floorHeight = 0.0f;
	};

	struct GridDynamicCell
	{
		uint16_t playerCount = 0;
		uint16_t monsterCount = 0;
		uint16_t arrowCount = 0;
		uint16_t bulletCount = 0;
	};

	enum class EDynamicKind : uint8_t
	{
		Player,
		Monster,
		Arrow,
		Bullet
	};

	struct DynamicTracker
	{
		CGameObject* object = nullptr;
		int prevCellX = -1;
		int prevCellZ = -1;
		bool occupied = false;
	};

public:
	void Initialize();
	void Shutdown();
	bool IsInitialized() const { return m_initialized; }

	void InitializeMegaGridState();

	bool WorldToCell(float worldX, float worldZ, int& outCellX, int& outCellZ) const;
	int GridCellIndex(int cellX, int cellZ) const;

	int MegaGridIndex(int megaX, int megaZ) const;
	int MegaGridNumberFromCell(int cellX, int cellZ) const;

	bool FineCellToMegaGridCell(int cellX, int cellZ, int& outMegaX, int& outMegaZ) const;
	bool IsFineCellInsideMegaGridApproachZone(int megaX, int megaZ, int cellX, int cellZ) const;

	void SetMegaGridApproachZoneSize(int megaX, int megaZ, int widthCells, int heightCells);
	void SetMegaGridCleared(int megaX, int megaZ, bool cleared);
	void SetMegaGridEventOccurred(int megaX, int megaZ, bool occurred);
	void SetMegaGridPlayerApproached(int megaX, int megaZ, bool approached);

	bool HasMegaGridPlayerApproached(int megaX, int megaZ) const;
	bool IsMegaGridCleared(int megaX, int megaZ) const;
	bool HasMegaGridEventOccurred(int megaX, int megaZ) const;

	bool IsFineCellInsideTreeCullVillageCenter(int megaX, int megaZ, int cellX, int cellZ) const;
	bool IsFineCellInsideTreeCullInnerBlockedArea(int megaX, int megaZ, int cellX, int cellZ) const;
	bool IsFineCellInsideLooseTreeVisibleGateZone(int megaX, int megaZ, int cellX, int cellZ) const;

	bool IsTreeCullBlockerCell(int cellX, int cellZ) const;
	bool RaycastTreeCullGridClear(int startCellX, int startCellZ, int endCellX, int endCellZ) const;
	bool CanFineCellSeeOutsideThroughVillageGate(int megaX, int megaZ, int cellX, int cellZ) const;
	bool ShouldCullTreesByVillageGridCell(int megaX, int megaZ, int cellX, int cellZ) const;

	void AddDynamicCount(int cellX, int cellZ, EDynamicKind kind, int delta);
	void ResetDynamicCounts();

	void StampBuildingCellsFromOOBB(
		const DirectX::BoundingOrientedBox& box,
		std::unordered_set<int>& touchedCells) const;

	void AddStaticTouchedCells(const std::unordered_set<int>& touchedCells);
	void MarkTreeCullBlockerCells(const std::unordered_set<int>& touchedCells);

	void DumpStaticGridOccupancyLog() const;

private:
	bool IsValidCell(int cellX, int cellZ) const;
	bool IsValidMegaGrid(int megaX, int megaZ) const;

private:
	bool m_initialized = false;

	std::vector<GridStaticCell> m_staticCells;
	std::vector<uint8_t> m_treeCullBlockerCells;
	std::vector<GridDynamicCell> m_dynamicCells;

	std::array<MegaGridCell, kMegaGridCount> m_megaGridCells = {};
};