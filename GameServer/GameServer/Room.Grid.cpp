#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "Enemy.h"
#include "Building.h"
#include "ColliderComponent.h"
#include "Projectile.h"

#include <cmath>

namespace
{
	static bool ShouldCreateWorldStaticCollider(Protocol::BuildingType type)
	{
		switch (type)
		{
		case Protocol::BUILDING_TYPE_GRASS:
		case Protocol::BUILDING_TYPE_GROUND:
		case Protocol::BUILDING_TYPE_DIRT_ROAD:
			return false;
		default:
			return true;
		}
	}
}

void Room::InitializeMegaGridState()
{
	for (MegaGridCell& cell : m_megaGridCells)
		cell = MegaGridCell{};
}

void Room::InitializeSpatialGrid()
{
	m_gridStaticCells.assign(kGridCellCount, GridStaticCell{});
	m_gridDynamicCells.assign(kGridCellCount, GridDynamicCell{});
	InitializeMegaGridState();

	m_playerGridTrackers.clear();
	m_monsterGridTrackers.clear();
	m_arrowGridTrackers.clear();
	m_bulletGridTrackers.clear();

	m_spatialGridInitialized = true;
}

void Room::ShutdownSpatialGrid()
{
	m_gridStaticCells.clear();
	m_gridDynamicCells.clear();
	InitializeMegaGridState();

	m_playerGridTrackers.clear();
	m_monsterGridTrackers.clear();
	m_arrowGridTrackers.clear();
	m_bulletGridTrackers.clear();

	m_spatialGridInitialized = false;
}

bool Room::WorldToGridCell(float worldX, float worldZ, int& outCellX, int& outCellZ) const
{
	if (worldX < static_cast<float>(kGridMinX) || worldX > static_cast<float>(kGridMaxX)) return false;
	if (worldZ < static_cast<float>(kGridMinZ) || worldZ > static_cast<float>(kGridMaxZ)) return false;

	int cellX = static_cast<int>(std::floor(worldX)) - kGridMinX;
	int cellZ = static_cast<int>(std::floor(worldZ)) - kGridMinZ;

	if (cellX == kGridWidth) cellX = kGridWidth - 1;
	if (cellZ == kGridHeight) cellZ = kGridHeight - 1;

	if (cellX < 0 || cellX >= kGridWidth) return false;
	if (cellZ < 0 || cellZ >= kGridHeight) return false;

	outCellX = cellX;
	outCellZ = cellZ;
	return true;
}

int Room::GridCellIndex(int cellX, int cellZ) const
{
	return (cellZ * kGridWidth) + cellX;
}

int Room::MegaGridIndex(int megaX, int megaZ) const
{
	return (megaZ * kMegaGridCols) + megaX;
}

bool Room::FineCellToMegaGridCell(int cellX, int cellZ, int& outMegaX, int& outMegaZ) const
{
	if (cellX < 0 || cellX >= kGridWidth) return false;
	if (cellZ < 0 || cellZ >= kGridHeight) return false;

	outMegaX = cellX / kMegaGridCellWidth;
	outMegaZ = cellZ / kMegaGridCellHeight;

	if (outMegaX < 0 || outMegaX >= kMegaGridCols) return false;
	if (outMegaZ < 0 || outMegaZ >= kMegaGridRows) return false;

	return true;
}

bool Room::IsFineCellInsideMegaGridApproachZone(int megaX, int megaZ, int cellX, int cellZ) const
{
	if (megaX < 0 || megaX >= kMegaGridCols) return false;
	if (megaZ < 0 || megaZ >= kMegaGridRows) return false;
	if (cellX < 0 || cellX >= kGridWidth) return false;
	if (cellZ < 0 || cellZ >= kGridHeight) return false;

	const MegaGridCell& megaCell = m_megaGridCells[static_cast<size_t>(MegaGridIndex(megaX, megaZ))];

	const int zoneWidth = std::clamp(megaCell.approachWidthCells, 1, kMegaGridCellWidth);
	const int zoneHeight = std::clamp(megaCell.approachHeightCells, 1, kMegaGridCellHeight);

	const int megaStartX = megaX * kMegaGridCellWidth;
	const int megaStartZ = megaZ * kMegaGridCellHeight;

	const int zoneStartX = megaStartX + ((kMegaGridCellWidth - zoneWidth) / 2);
	const int zoneStartZ = megaStartZ + ((kMegaGridCellHeight - zoneHeight) / 2);

	const int zoneEndX = zoneStartX + zoneWidth;
	const int zoneEndZ = zoneStartZ + zoneHeight;

	return
		(cellX >= zoneStartX && cellX < zoneEndX) &&
		(cellZ >= zoneStartZ && cellZ < zoneEndZ);
}

void Room::AddDynamicCount(int cellX, int cellZ, EGridDynamicKind kind, int delta)
{
	if (!m_spatialGridInitialized) return;
	if (cellX < 0 || cellX >= kGridWidth) return;
	if (cellZ < 0 || cellZ >= kGridHeight) return;

	GridDynamicCell& cell = m_gridDynamicCells[static_cast<size_t>(GridCellIndex(cellX, cellZ))];

	uint16_t* target = nullptr;
	switch (kind)
	{
	case EGridDynamicKind::Player: target = &cell.playerCount; break;
	case EGridDynamicKind::Monster: target = &cell.monsterCount; break;
	case EGridDynamicKind::Arrow: target = &cell.arrowCount; break;
	case EGridDynamicKind::Bullet: target = &cell.bulletCount; break;
	default: return;
	}

	int next = static_cast<int>(*target) + delta;
	if (next < 0) next = 0;
	*target = static_cast<uint16_t>(next);
}

void Room::RegisterStaticBuildingToGrid(BuildingRef building)
{
	if (!m_spatialGridInitialized || !building)
		return;

	if (!ShouldCreateWorldStaticCollider(building->GetBuildingType()))
		return;

	std::unordered_set<int> touchedCells;

	auto StampBuildingCellsFromOOBB = [&](const BoundingOrientedBox& box)
		{
			XMFLOAT3 corners[8] = {};
			box.GetCorners(corners);

			float minX = corners[0].x;
			float maxX = corners[0].x;
			float minZ = corners[0].z;
			float maxZ = corners[0].z;

			for (int i = 1; i < 8; ++i)
			{
				if (corners[i].x < minX) minX = corners[i].x;
				if (corners[i].x > maxX) maxX = corners[i].x;
				if (corners[i].z < minZ) minZ = corners[i].z;
				if (corners[i].z > maxZ) maxZ = corners[i].z;
			}

			int beginCellX = static_cast<int>(std::floor(minX)) - kGridMinX;
			int endCellX = static_cast<int>(std::ceil(maxX)) - kGridMinX - 1;

			int beginCellZ = static_cast<int>(std::floor(minZ)) - kGridMinZ;
			int endCellZ = static_cast<int>(std::ceil(maxZ)) - kGridMinZ - 1;

			if (beginCellX < 0) beginCellX = 0;
			if (beginCellZ < 0) beginCellZ = 0;
			if (endCellX >= kGridWidth) endCellX = kGridWidth - 1;
			if (endCellZ >= kGridHeight) endCellZ = kGridHeight - 1;

			if (beginCellX > endCellX) return;
			if (beginCellZ > endCellZ) return;

			for (int z = beginCellZ; z <= endCellZ; ++z)
			{
				for (int x = beginCellX; x <= endCellX; ++x)
				{
					touchedCells.insert(GridCellIndex(x, z));
				}
			}
		};

	if (auto* collider = building->GetComponent<CColliderComponent>())
	{
		for (const auto& subOOBB : collider->GetSubOOBBs())
			StampBuildingCellsFromOOBB(subOOBB);

		if (touchedCells.empty() && collider->GetType() == EColliderType::OOBB)
			StampBuildingCellsFromOOBB(collider->GetOOBB());
	}

	if (touchedCells.empty())
	{
		int cellX = -1;
		int cellZ = -1;
		const GameMath::Vec3 pos = building->GetPosition();

		if (WorldToGridCell(pos.x, pos.z, cellX, cellZ))
			touchedCells.insert(GridCellIndex(cellX, cellZ));
	}

	for (int cellIndex : touchedCells)
	{
		if (cellIndex < 0 || cellIndex >= kGridCellCount) continue;
		++m_gridStaticCells[static_cast<size_t>(cellIndex)].buildingCount;
		m_gridStaticCells[static_cast<size_t>(cellIndex)].floorHeight = 0.0f;
	}
}

void Room::ResetDynamicGridCounts()
{
	for (GridDynamicCell& cell : m_gridDynamicCells)
	{
		cell.playerCount = 0;
		cell.monsterCount = 0;
		cell.arrowCount = 0;
		cell.bulletCount = 0;
	}
}

bool Room::TryGetTrackedCell(const CServerObject* obj, int& outCellX, int& outCellZ) const
{
	if (!obj) return false;
	if (!obj->IsActive()) return false;

	if (const auto* projectile = dynamic_cast<const CProjectile*>(obj))
	{
		if (!projectile->IsActive())
			return false;
	}

	const GameMath::Vec3 pos = obj->GetPosition();
	if (pos.y < -100.0f) return false;

	return WorldToGridCell(pos.x, pos.z, outCellX, outCellZ);
}

void Room::RefreshDynamicTracker(GridDynamicTracker& tracker, EGridDynamicKind kind)
{
	int currentCellX = -1;
	int currentCellZ = -1;
	const bool hasCurrentCell = TryGetTrackedCell(tracker.object, currentCellX, currentCellZ);

	if (tracker.occupied)
	{
		if (!hasCurrentCell || tracker.prevCellX != currentCellX || tracker.prevCellZ != currentCellZ)
		{
			AddDynamicCount(tracker.prevCellX, tracker.prevCellZ, kind, -1);
			tracker.occupied = false;
		}
	}

	if (hasCurrentCell)
	{
		if (!tracker.occupied || tracker.prevCellX != currentCellX || tracker.prevCellZ != currentCellZ)
		{
			AddDynamicCount(currentCellX, currentCellZ, kind, +1);
			tracker.prevCellX = currentCellX;
			tracker.prevCellZ = currentCellZ;
			tracker.occupied = true;
		}
	}
}

void Room::BuildDynamicGridTrackers()
{
	m_playerGridTrackers.clear();
	m_playerGridTrackers.reserve(players.size());
	for (auto& p : players)
	{
		GridDynamicTracker tracker{};
		tracker.object = p.second.get();
		m_playerGridTrackers.push_back(tracker);
	}

	m_monsterGridTrackers.clear();
	m_monsterGridTrackers.reserve(enemies.size());
	for (auto& e : enemies)
	{
		GridDynamicTracker tracker{};
		tracker.object = e.second.get();
		m_monsterGridTrackers.push_back(tracker);
	}

	m_arrowGridTrackers.clear();
	m_arrowGridTrackers.reserve(m_arrowPool.size());
	for (auto& p : m_arrowPool)
	{
		GridDynamicTracker tracker{};
		tracker.object = p.get();
		m_arrowGridTrackers.push_back(tracker);
	}

	m_bulletGridTrackers.clear();
	m_bulletGridTrackers.reserve(m_bulletPool.size());
	for (auto& p : m_bulletPool)
	{
		GridDynamicTracker tracker{};
		tracker.object = p.get();
		m_bulletGridTrackers.push_back(tracker);
	}
}

void Room::RebuildDynamicGridState()
{
	if (!m_spatialGridInitialized) return;

	ResetDynamicGridCounts();
	BuildDynamicGridTrackers();

	for (auto& tracker : m_playerGridTrackers)
		RefreshDynamicTracker(tracker, EGridDynamicKind::Player);

	for (auto& tracker : m_monsterGridTrackers)
		RefreshDynamicTracker(tracker, EGridDynamicKind::Monster);

	for (auto& tracker : m_arrowGridTrackers)
		RefreshDynamicTracker(tracker, EGridDynamicKind::Arrow);

	for (auto& tracker : m_bulletGridTrackers)
		RefreshDynamicTracker(tracker, EGridDynamicKind::Bullet);

	UpdateMegaGridState();
}

void Room::UpdateDynamicGridState()
{
	if (!m_spatialGridInitialized) return;

	for (auto& tracker : m_playerGridTrackers)
		RefreshDynamicTracker(tracker, EGridDynamicKind::Player);

	for (auto& tracker : m_monsterGridTrackers)
		RefreshDynamicTracker(tracker, EGridDynamicKind::Monster);

	for (auto& tracker : m_arrowGridTrackers)
		RefreshDynamicTracker(tracker, EGridDynamicKind::Arrow);

	for (auto& tracker : m_bulletGridTrackers)
		RefreshDynamicTracker(tracker, EGridDynamicKind::Bullet);

	UpdateMegaGridState();
}

void Room::UpdateMegaGridState()
{
	if (!m_spatialGridInitialized) return;

	for (const GridDynamicTracker& tracker : m_playerGridTrackers)
	{
		if (!tracker.occupied) continue;

		int megaX = -1;
		int megaZ = -1;
		if (!FineCellToMegaGridCell(tracker.prevCellX, tracker.prevCellZ, megaX, megaZ))
			continue;

		if (!IsFineCellInsideMegaGridApproachZone(megaX, megaZ, tracker.prevCellX, tracker.prevCellZ))
			continue;

		MegaGridCell& megaCell = m_megaGridCells[static_cast<size_t>(MegaGridIndex(megaX, megaZ))];
		megaCell.hasPlayerApproached = true;
	}
}
