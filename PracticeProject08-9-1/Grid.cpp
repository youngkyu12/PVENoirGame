//-----------------------------------------------------------------------------
// File: Grid.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Grid.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

void CSceneGrid::Initialize()
{
	m_staticCells.assign(kGridCellCount, GridStaticCell{});
	m_treeCullBlockerCells.assign(kGridCellCount, 0);
	m_dynamicCells.assign(kGridCellCount, GridDynamicCell{});
	InitializeMegaGridState();

	m_initialized = true;
}

void CSceneGrid::Shutdown()
{
	m_staticCells.clear();
	m_treeCullBlockerCells.clear();
	m_dynamicCells.clear();
	InitializeMegaGridState();

	m_initialized = false;
}

void CSceneGrid::InitializeMegaGridState()
{
	for ( MegaGridCell& cell : m_megaGridCells )
		cell = MegaGridCell{};
}

bool CSceneGrid::IsValidCell(int cellX, int cellZ) const
{
	return
		( cellX >= 0 && cellX < kGridWidth ) &&
		( cellZ >= 0 && cellZ < kGridHeight );
}

bool CSceneGrid::IsValidMegaGrid(int megaX, int megaZ) const
{
	return
		( megaX >= 0 && megaX < kMegaGridCols ) &&
		( megaZ >= 0 && megaZ < kMegaGridRows );
}

bool CSceneGrid::WorldToCell(float worldX, float worldZ, int& outCellX, int& outCellZ) const
{
	if ( worldX < static_cast< float >(kGridMinX) || worldX > static_cast< float >(kGridMaxX) )
		return false;

	if ( worldZ < static_cast< float >(kGridMinZ) || worldZ > static_cast< float >(kGridMaxZ) )
		return false;

	int cellX = static_cast< int >( std::floor(worldX) ) - kGridMinX;
	int cellZ = static_cast< int >( std::floor(worldZ) ) - kGridMinZ;

	if ( cellX == kGridWidth )
		cellX = kGridWidth - 1;

	if ( cellZ == kGridHeight )
		cellZ = kGridHeight - 1;

	if ( !IsValidCell(cellX, cellZ) )
		return false;

	outCellX = cellX;
	outCellZ = cellZ;
	return true;
}

int CSceneGrid::GridCellIndex(int cellX, int cellZ) const
{
	return ( cellZ * kGridWidth ) + cellX;
}

int CSceneGrid::MegaGridIndex(int megaX, int megaZ) const
{
	return ( megaZ * kMegaGridCols ) + megaX;
}

int CSceneGrid::MegaGridNumberFromCell(int cellX, int cellZ) const
{
	int megaX = -1;
	int megaZ = -1;

	if ( !FineCellToMegaGridCell(cellX, cellZ, megaX, megaZ) )
		return -1;

	return ( megaZ * kMegaGridCols ) + megaX + 1;
}

int CSceneGrid::MegaGridNumberFromWorldPosition(float worldX, float worldZ) const
{
	int cellX = -1;
	int cellZ = -1;

	if ( !WorldToCell(worldX, worldZ, cellX, cellZ) )
		return -1;

	return MegaGridNumberFromCell(cellX, cellZ);
}

bool CSceneGrid::FineCellToMegaGridCell(int cellX, int cellZ, int& outMegaX, int& outMegaZ) const
{
	if ( !IsValidCell(cellX, cellZ) )
		return false;

	outMegaX = cellX / kMegaGridCellWidth;
	outMegaZ = cellZ / kMegaGridCellHeight;

	return IsValidMegaGrid(outMegaX, outMegaZ);
}

bool CSceneGrid::TryGetMegaGridFromWorldPosition(
	float worldX,
	float worldZ,
	int& outMegaX,
	int& outMegaZ) const
{
	int cellX = -1;
	int cellZ = -1;

	if ( !WorldToCell(worldX, worldZ, cellX, cellZ) )
		return false;

	return FineCellToMegaGridCell(cellX, cellZ, outMegaX, outMegaZ);
}

bool CSceneGrid::IsFineCellInsideMegaGridApproachZone(
	int megaX,
	int megaZ,
	int cellX,
	int cellZ) const
{
	if ( !IsValidMegaGrid(megaX, megaZ) )
		return false;

	if ( !IsValidCell(cellX, cellZ) )
		return false;

	const MegaGridCell& megaCell = m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)];

	const int zoneWidth =
		std::clamp(megaCell.approachWidthCells, 1, kMegaGridCellWidth);

	const int zoneHeight =
		std::clamp(megaCell.approachHeightCells, 1, kMegaGridCellHeight);

	const int megaStartX = megaX * kMegaGridCellWidth;
	const int megaStartZ = megaZ * kMegaGridCellHeight;

	const int zoneStartX = megaStartX + ( ( kMegaGridCellWidth - zoneWidth ) / 2 );
	const int zoneStartZ = megaStartZ + ( ( kMegaGridCellHeight - zoneHeight ) / 2 );

	const int zoneEndX = zoneStartX + zoneWidth;
	const int zoneEndZ = zoneStartZ + zoneHeight;

	return
		( cellX >= zoneStartX && cellX < zoneEndX ) &&
		( cellZ >= zoneStartZ && cellZ < zoneEndZ );
}

void CSceneGrid::SetMegaGridApproachZoneSize(int megaX, int megaZ, int widthCells, int heightCells)
{
	if ( !IsValidMegaGrid(megaX, megaZ) )
		return;

	MegaGridCell& cell = m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)];
	cell.approachWidthCells = std::clamp(widthCells, 1, kMegaGridCellWidth);
	cell.approachHeightCells = std::clamp(heightCells, 1, kMegaGridCellHeight);
}

void CSceneGrid::SetMegaGridCleared(int megaX, int megaZ, bool cleared)
{
	if ( !IsValidMegaGrid(megaX, megaZ) )
		return;

	m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)].isCleared = cleared;
}

void CSceneGrid::SetMegaGridEventOccurred(int megaX, int megaZ, bool occurred)
{
	if ( !IsValidMegaGrid(megaX, megaZ) )
		return;

	m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)].hasEventOccurred = occurred;
}

void CSceneGrid::SetMegaGridPlayerApproached(int megaX, int megaZ, bool approached)
{
	if ( !IsValidMegaGrid(megaX, megaZ) )
		return;

	m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)].hasPlayerApproached = approached;
}

void CSceneGrid::ClearMegaGridMonsters()
{
	for ( MegaGridCell& cell : m_megaGridCells )
		cell.monsterObjects.clear();
}

void CSceneGrid::AddMonsterToMegaGrid(int megaX, int megaZ, CGameObject* monster)
{
	if ( !monster )
		return;

	if ( !IsValidMegaGrid(megaX, megaZ) )
		return;

	std::vector<CGameObject*>& monsters =
		m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)].monsterObjects;

	if ( std::find(monsters.begin(), monsters.end(), monster) != monsters.end() )
		return;

	monsters.push_back(monster);
}

const std::vector<CGameObject*>& CSceneGrid::GetMegaGridMonsters(int megaX, int megaZ) const
{
	static const std::vector<CGameObject*> kEmpty;

	if ( !IsValidMegaGrid(megaX, megaZ) )
		return kEmpty;

	return m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)].monsterObjects;
}

bool CSceneGrid::HasMegaGridPlayerApproached(int megaX, int megaZ) const
{
	if ( !IsValidMegaGrid(megaX, megaZ) )
		return false;

	return m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)].hasPlayerApproached;
}

bool CSceneGrid::IsMegaGridCleared(int megaX, int megaZ) const
{
	if ( !IsValidMegaGrid(megaX, megaZ) )
		return false;

	return m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)].isCleared;
}

bool CSceneGrid::HasMegaGridEventOccurred(int megaX, int megaZ) const
{
	if ( !IsValidMegaGrid(megaX, megaZ) )
		return false;

	return m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)].hasEventOccurred;
}

bool CSceneGrid::IsFineCellInsideTreeCullVillageCenter(
	int megaX,
	int megaZ,
	int cellX,
	int cellZ) const
{
	if ( !IsValidMegaGrid(megaX, megaZ) )
		return false;

	if ( !IsValidCell(cellX, cellZ) )
		return false;

	const int centerSize = kTreeCullVillageCenterSizeCells;

	const int megaStartX = megaX * kMegaGridCellWidth;
	const int megaStartZ = megaZ * kMegaGridCellHeight;

	const int centerStartX =
		megaStartX + ( ( kMegaGridCellWidth - centerSize ) / 2 );

	const int centerStartZ =
		megaStartZ + ( ( kMegaGridCellHeight - centerSize ) / 2 );

	const int centerEndX = centerStartX + centerSize;
	const int centerEndZ = centerStartZ + centerSize;

	return
		( cellX >= centerStartX && cellX < centerEndX ) &&
		( cellZ >= centerStartZ && cellZ < centerEndZ );
}

bool CSceneGrid::IsFineCellInsideTreeCullInnerBlockedArea(
	int megaX,
	int megaZ,
	int cellX,
	int cellZ) const
{
	if ( !IsValidMegaGrid(megaX, megaZ) )
		return false;

	if ( !IsValidCell(cellX, cellZ) )
		return false;

	const int blockedSize = kTreeCullVillageInnerBlockedSizeCells;

	const int megaStartX = megaX * kMegaGridCellWidth;
	const int megaStartZ = megaZ * kMegaGridCellHeight;

	const int blockedStartX =
		megaStartX + ( ( kMegaGridCellWidth - blockedSize ) / 2 );

	const int blockedStartZ =
		megaStartZ + ( ( kMegaGridCellHeight - blockedSize ) / 2 );

	const int blockedEndX = blockedStartX + blockedSize;
	const int blockedEndZ = blockedStartZ + blockedSize;

	return
		( cellX >= blockedStartX && cellX < blockedEndX ) &&
		( cellZ >= blockedStartZ && cellZ < blockedEndZ );
}

bool CSceneGrid::IsFineCellInsideLooseTreeVisibleGateZone(
	int megaX,
	int megaZ,
	int cellX,
	int cellZ) const
{
	if ( !IsFineCellInsideTreeCullVillageCenter(megaX, megaZ, cellX, cellZ) )
		return false;

	const int centerSize = kTreeCullVillageCenterSizeCells;

	const int megaStartX = megaX * kMegaGridCellWidth;
	const int megaStartZ = megaZ * kMegaGridCellHeight;

	const int centerStartX =
		megaStartX + ( ( kMegaGridCellWidth - centerSize ) / 2 );

	const int centerStartZ =
		megaStartZ + ( ( kMegaGridCellHeight - centerSize ) / 2 );

	const float localX =
		static_cast< float >( cellX - centerStartX ) + 0.5f - kTreeCullVillageHalfSize;

	const float localZ =
		static_cast< float >( cellZ - centerStartZ ) + 0.5f - kTreeCullVillageHalfSize;

	const float half = kTreeCullVillageHalfSize;
	const float gateHalf =
		static_cast< float >( kTreeCullLooseGateHalfWidthCells );

	const float insideDepth =
		static_cast< float >( kTreeCullLooseGateInsideDepthCells );

	const bool nearNorthGate =
		( std::fabs(localX) <= gateHalf ) &&
		( localZ >= half - insideDepth );

	const bool nearSouthGate =
		( std::fabs(localX) <= gateHalf ) &&
		( localZ <= -half + insideDepth );

	const bool nearEastGate =
		( std::fabs(localZ) <= gateHalf ) &&
		( localX >= half - insideDepth );

	const bool nearWestGate =
		( std::fabs(localZ) <= gateHalf ) &&
		( localX <= -half + insideDepth );

	return
		nearNorthGate ||
		nearSouthGate ||
		nearEastGate ||
		nearWestGate;
}

bool CSceneGrid::IsTreeCullBlockerCell(int cellX, int cellZ) const
{
	if ( !IsValidCell(cellX, cellZ) )
		return true;

	const int cellIndex = GridCellIndex(cellX, cellZ);

	if ( cellIndex < 0 || cellIndex >= kGridCellCount )
		return true;

	if ( cellIndex >= static_cast< int >(m_treeCullBlockerCells.size()) )
		return false;

	return m_treeCullBlockerCells[( size_t ) cellIndex] != 0;
}

bool CSceneGrid::RaycastTreeCullGridClear(
	int startCellX,
	int startCellZ,
	int endCellX,
	int endCellZ) const
{
	if ( !IsValidCell(startCellX, startCellZ) )
		return false;

	if ( !IsValidCell(endCellX, endCellZ) )
		return false;

	int x0 = startCellX;
	int z0 = startCellZ;
	const int x1 = endCellX;
	const int z1 = endCellZ;

	const int dx = ( x1 >= x0 ) ? ( x1 - x0 ) : ( x0 - x1 );
	const int dzAbs = ( z1 >= z0 ) ? ( z1 - z0 ) : ( z0 - z1 );

	const int stepX = ( x0 < x1 ) ? 1 : -1;
	const int stepZ = ( z0 < z1 ) ? 1 : -1;

	const int dz = -dzAbs;
	int error = dx + dz;

	bool firstCell = true;

	for ( ;; )
	{
		if ( !firstCell )
		{
			if ( IsTreeCullBlockerCell(x0, z0) )
				return false;
		}

		if ( x0 == x1 && z0 == z1 )
			break;

		const int error2 = error * 2;

		if ( error2 >= dz )
		{
			error += dz;
			x0 += stepX;
		}

		if ( error2 <= dx )
		{
			error += dx;
			z0 += stepZ;
		}

		firstCell = false;
	}

	return true;
}

bool CSceneGrid::CanFineCellSeeOutsideThroughVillageGate(int megaX, int megaZ, int cellX, int cellZ) const
{
	UNREFERENCED_PARAMETER(megaX);
	UNREFERENCED_PARAMETER(megaZ);
	UNREFERENCED_PARAMETER(cellX);
	UNREFERENCED_PARAMETER(cellZ);
	return true;
}

bool CSceneGrid::ShouldCullTreesByVillageGridCell(int megaX, int megaZ, int cellX, int cellZ) const
{
	UNREFERENCED_PARAMETER(megaX);
	UNREFERENCED_PARAMETER(megaZ);
	UNREFERENCED_PARAMETER(cellX);
	UNREFERENCED_PARAMETER(cellZ);
	return false;
}

void CSceneGrid::AddDynamicCount(int cellX, int cellZ, EDynamicKind kind, int delta)
{
	if ( !m_initialized )
		return;

	if ( !IsValidCell(cellX, cellZ) )
		return;

	GridDynamicCell& cell = m_dynamicCells[( size_t ) GridCellIndex(cellX, cellZ)];

	uint16_t* target = nullptr;

	switch ( kind )
	{
	case EDynamicKind::Player:
		target = &cell.playerCount;
		break;

	case EDynamicKind::Monster:
		target = &cell.monsterCount;
		break;

	case EDynamicKind::Arrow:
		target = &cell.arrowCount;
		break;

	case EDynamicKind::Bullet:
		target = &cell.bulletCount;
		break;

	default:
		return;
	}

	int nextValue = static_cast< int >( *target ) + delta;

	if ( nextValue < 0 )
		nextValue = 0;

	*target = static_cast< uint16_t >(nextValue);
}

void CSceneGrid::ResetDynamicCounts()
{
	for ( GridDynamicCell& cell : m_dynamicCells )
	{
		cell.playerCount = 0;
		cell.monsterCount = 0;
		cell.arrowCount = 0;
		cell.bulletCount = 0;
	}
}

void CSceneGrid::StampBuildingCellsFromOOBB(
	const BoundingOrientedBox& box,
	std::unordered_set<int>& touchedCells) const
{
	XMFLOAT3 corners[8] = {};
	box.GetCorners(corners);

	float minX = corners[0].x;
	float maxX = corners[0].x;
	float minZ = corners[0].z;
	float maxZ = corners[0].z;

	for ( int i = 1; i < 8; ++i )
	{
		if ( corners[i].x < minX ) minX = corners[i].x;
		if ( corners[i].x > maxX ) maxX = corners[i].x;
		if ( corners[i].z < minZ ) minZ = corners[i].z;
		if ( corners[i].z > maxZ ) maxZ = corners[i].z;
	}

	int beginCellX = static_cast< int >(std::floor(minX)) - kGridMinX;
	int endCellX = static_cast< int >(std::ceil(maxX)) - kGridMinX - 1;

	int beginCellZ = static_cast< int >( std::floor(minZ) ) - kGridMinZ;
	int endCellZ = static_cast< int >( std::ceil(maxZ) ) - kGridMinZ - 1;

	if ( beginCellX < 0 ) beginCellX = 0;
	if ( beginCellZ < 0 ) beginCellZ = 0;
	if ( endCellX >= kGridWidth ) endCellX = kGridWidth - 1;
	if ( endCellZ >= kGridHeight ) endCellZ = kGridHeight - 1;

	if ( beginCellX > endCellX )
		return;

	if ( beginCellZ > endCellZ )
		return;

	for ( int z = beginCellZ; z <= endCellZ; ++z )
	{
		for ( int x = beginCellX; x <= endCellX; ++x )
			touchedCells.insert(GridCellIndex(x, z));
	}
}

void CSceneGrid::AddStaticTouchedCells(const std::unordered_set<int>& touchedCells)
{
	for ( int cellIndex : touchedCells )
	{
		if ( cellIndex < 0 || cellIndex >= kGridCellCount )
			continue;

		if ( cellIndex >= static_cast< int >(m_staticCells.size()) )
			continue;

		++m_staticCells[( size_t ) cellIndex].buildingCount;
		m_staticCells[( size_t ) cellIndex].floorHeight = 0.0f;
	}
}

void CSceneGrid::MarkTreeCullBlockerCells(const std::unordered_set<int>& touchedCells)
{
	UNREFERENCED_PARAMETER(touchedCells);
}

bool CSceneGrid::IsStaticBuildingCell(int cellX, int cellZ) const
{
	if ( !m_initialized )
		return true;

	if ( !IsValidCell(cellX, cellZ) )
		return true;

	const int cellIndex = GridCellIndex(cellX, cellZ);

	if ( cellIndex < 0 || cellIndex >= kGridCellCount )
		return true;

	if ( cellIndex >= static_cast< int >(m_staticCells.size()) )
		return true;

	return m_staticCells[static_cast< size_t >(cellIndex)].buildingCount > 0;
}

void CSceneGrid::DumpStaticGridOccupancyLog() const
{
	if ( !m_initialized )
	{
		OutputDebugStringA("[GridStatic] not initialized\n");
		return;
	}

	OutputDebugStringA("[GridStatic] begin\n");

	std::string row;
	row.reserve(kGridWidth + 1);

	for ( int z = 0; z < kGridHeight; ++z )
	{
		row.clear();

		for ( int x = 0; x < kGridWidth; ++x )
		{
			const GridStaticCell& cell =
				m_staticCells[( size_t ) GridCellIndex(x, z)];

			row.push_back(( cell.buildingCount > 0 ) ? '1' : '0');
		}

		row.push_back('\n');
		OutputDebugStringA(row.c_str());
	}

	OutputDebugStringA("[GridStatic] end\n");
}