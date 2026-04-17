//-----------------------------------------------------------------------------
// File: GameScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScene.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <cstdio>
#include <cctype>
#include <unordered_map>
#include <algorithm>

#include "AnimatorComponent.h"
#include "AnimatorData.h"
#include "AnimController.h"
#include "MonsterAnimController.h"
#include "MonsterAnimTypes.h"
#include "Material.h"
#include "AssetManager.h"
#include "Texture.h"
#include "LightComponent.h"
#include "PlayerControllerComponent.h"
#include "Object.h"
#include "SkinningComponent.h"
#include "ActorTagComponent.h"
#include "ArrowComponent.h"
#include "BulletComponent.h"
#include "Camera.h"
#include "FollowBoneComponent.h"
#include "PlayerEquipmentComponent.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"
#include "WeaponHitboxComponent.h"
#include "MonsterWeaponHitboxComponent.h"
#include "MonsterCombatComponent.h"
#include "NavMesh.h"
#include "GhoulAIComponent.h"
#include "AudioManager.h"
#include "MusicDirector.h"

#include "ThreadManager.h"
#include "Service.h"
#include "Session.h"
#include "BufferReader.h"
#include "ServerPacketHandler.h"

#include "GlobalValues.h"
#include "GameSceneContentCatalog.h"
#include "GameSceneObjectFactory.h"
#include "GameSceneAttachmentBinder.h"

namespace
{
    // [die][hit][run][roll][attack][up][down][left][right][move]
    // move를 bit0(LSB)로 가정
    enum : uint32_t
    {
        kStateMoveBit = 1u << 0,
        kStateRightBit = 1u << 1,
        kStateLeftBit = 1u << 2,
        kStateDownBit = 1u << 3,
        kStateUpBit = 1u << 4,
        kStateAttackBit = 1u << 5,
        kStateRollBit = 1u << 6,
        kStateRunBit = 1u << 7,
        kStateHitBit = 1u << 8,
        kStateDieBit = 1u << 9,
    };

	enum : uint32_t
	{
		kCollisionLayerPlayer = 0, // 플레이어 본체
		kCollisionLayerMonster = 1, // 몬스터 본체
		kCollisionLayerWorldStatic = 2, // 월드 정적 오브젝트
		kCollisionLayerPlayerWeapon = 3, // 플레이어 무기
		kCollisionLayerMonsterWeapon = 4  // 몬스터 무기
	};

	static constexpr uint32_t CollisionBit(uint32_t layer)
	{
		return ( 1u << layer );
	}

    struct DecodedAnimStateCode
    {
        bool hasMove = false;
        bool run = false;
        uint32_t moveDirBits = 0;

        bool die = false;
        bool hit = false;
        bool roll = false;
        bool attack = false;
    };

    static DecodedAnimStateCode DecodeStateCode(uint32_t stateCode)
    {
        DecodedAnimStateCode out{};

        const bool moveBit = (stateCode & kStateMoveBit) != 0;

        if (moveBit)
        {
            if (stateCode & kStateUpBit) out.moveDirBits |= DIR_FORWARD;
            if (stateCode & kStateDownBit) out.moveDirBits |= DIR_BACKWARD;
            if (stateCode & kStateLeftBit) out.moveDirBits |= DIR_LEFT;
            if (stateCode & kStateRightBit) out.moveDirBits |= DIR_RIGHT;

            // 규칙:
            // isMove=1이어도 방향 4비트가 모두 0이면 Move/Run 아님
            out.hasMove = (out.moveDirBits != 0);
            out.run = out.hasMove && ((stateCode & kStateRunBit) != 0);
        }

        // 우선순위: die > hit > roll > attack
        out.die = (stateCode & kStateDieBit) != 0;
        out.hit = !out.die && ((stateCode & kStateHitBit) != 0);
        out.roll = !out.die && !out.hit && ((stateCode & kStateRollBit) != 0);
        out.attack = !out.die && !out.hit && !out.roll && ((stateCode & kStateAttackBit) != 0);

        return out;
    }

	static constexpr UINT kDebugSubmeshOOBBCapacity = 8192;
	static constexpr bool kEnableStaticWorldLocalOOBBReportExport = false;
	static constexpr const char* kStaticWorldLocalOOBBReportPath = "MapData/StaticWorldLocalOOBBReport.txt";

	static XMFLOAT4X4 BuildWorldMatrixFromOOBB(const BoundingOrientedBox& box)
	{
		XMFLOAT4X4 out{};

		const XMMATRIX S = XMMatrixScaling(
			box.Extents.x * 2.0f,
			box.Extents.y * 2.0f,
			box.Extents.z * 2.0f
		);

		const XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&box.Orientation));

		const XMMATRIX T = XMMatrixTranslation(
			box.Center.x,
			box.Center.y,
			box.Center.z
		);

		XMStoreFloat4x4(&out, S * R * T);
		return out;
	}

	static XMFLOAT4X4 BuildIdentityMatrix4x4()
	{
		XMFLOAT4X4 out{};
		XMStoreFloat4x4(&out, XMMatrixIdentity());
		return out;
	}

	static constexpr ELocalStagePreset kLocalStagePreset = ELocalStagePreset::FullStage;
}

namespace
{
    bool ParsePlacementEntryLine(const std::string& line, StaticPlacementEntry& outEntry)
    {
        char asset[64] = {};
        char objectName[128] = {};

        float px = 0.0f, py = 0.0f, pz = 0.0f;
        float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f;

        const int matched = sscanf_s(
            line.c_str(),
            "ENTRY|asset=\"%63[^\"]\"|object=\"%127[^\"]\"|pos=(%f,%f,%f)|rot=(%f,%f,%f,%f)",
            asset, (unsigned)_countof(asset),
            objectName, (unsigned)_countof(objectName),
            &px, &py, &pz,
            &qx, &qy, &qz, &qw
        );

        if (matched != 9) return false;

        outEntry.assetName = asset;
        outEntry.objectName = objectName;
        outEntry.pos = XMFLOAT3(px, py, pz);
        outEntry.rot = XMFLOAT4(qx, qy, qz, qw);
        return true;
    }

	std::string TrimString(const std::string& text)
	{
		size_t begin = 0;
		while ( begin < text.size() && std::isspace(static_cast< unsigned char >(text[begin])) )
			++begin;

		size_t end = text.size();
		while ( end > begin && std::isspace(static_cast< unsigned char >( text[end - 1] )) )
			--end;

		return text.substr(begin, end - begin);
	}

	std::string StripTopRootFromAPath(const std::string& fullAPath)
	{
		const size_t firstSlash = fullAPath.find('/');
		if ( firstSlash == std::string::npos )
			return fullAPath;

		return fullAPath.substr(firstSlash + 1);
	}

	bool ParseVector3Tuple(const std::string& text, XMFLOAT3& outValue)
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;

		if ( sscanf_s(text.c_str(), "(%f, %f, %f)", &x, &y, &z) != 3 )
			return false;

		outValue = XMFLOAT3(x, y, z);
		return true;
	}

	bool ParseVector4Tuple(const std::string& text, XMFLOAT4& outValue)
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 1.0f;

		if ( sscanf_s(text.c_str(), "(%f, %f, %f, %f)", &x, &y, &z, &w) != 4 )
			return false;

		outValue = XMFLOAT4(x, y, z, w);
		return true;
	}

    static void BuildStaticPlacementsFromNetworkGameStart(
        const GameStartData& gameStartData,
        std::vector<StaticPlacementEntry>& outEntries)
    {
        outEntries.clear();
        outEntries.reserve(gameStartData.buildings.size());

        for (const auto& b : gameStartData.buildings)
        {
            if (b.assetName.empty())
                continue;

            StaticPlacementEntry entry{};
            entry.assetName = b.assetName;
            entry.objectName = "NetBuilding_" + std::to_string(b.id);
            entry.pos = b.position;
            entry.yawDeg = b.yaw;
            outEntries.push_back(std::move(entry));
        }
    }

	static int ClampStaticWorldLodLevel(int lodLevel)
	{
		if ( lodLevel < 0 ) return 0;
		if ( lodLevel > 2 ) return 2;
		return lodLevel;
	}

	static bool BuildStaticLodMeshBinPath(
		const std::string& baseMeshBinPath,
		int lodLevel,
		std::string& outMeshBinPath)
	{
		const size_t dotPos = baseMeshBinPath.find_last_of('.');
		if ( dotPos == std::string::npos )
			return false;

		const int clampedLodLevel = ClampStaticWorldLodLevel(lodLevel);

		outMeshBinPath = baseMeshBinPath.substr(0, dotPos);
		outMeshBinPath += "_LOD";
		outMeshBinPath += std::to_string(clampedLodLevel);
		outMeshBinPath += baseMeshBinPath.substr(dotPos);

		return true;
	}

	static int ClampSkinnedWorldLodLevel(int lodLevel)
	{
		if ( lodLevel < 0 ) return 0;
		if ( lodLevel > 2 ) return 2;
		return lodLevel;
	}

	static bool BuildSkinnedLodMeshBinPath(
		const std::string& baseMeshBinPath,
		int lodLevel,
		std::string& outMeshBinPath)
	{
		const size_t dotPos = baseMeshBinPath.find_last_of('.');
		if ( dotPos == std::string::npos )
			return false;

		const int clampedLodLevel = ClampSkinnedWorldLodLevel(lodLevel);

		outMeshBinPath = baseMeshBinPath.substr(0, dotPos);
		outMeshBinPath += "_LOD";
		outMeshBinPath += std::to_string(clampedLodLevel);
		outMeshBinPath += baseMeshBinPath.substr(dotPos);
		return true;
	}

	void TriggerMonsterTestCommand(CGameObject* obj, EMonsterAnimCommand cmd, EMonsterAnimState locomotion = EMonsterAnimState::Idle)
	{
		if ( !obj ) return;

		auto* animComp = obj->GetComponent<CAnimatorComponent>();
		if ( !animComp ) return;

		auto* ctrl = animComp->EnsureMonsterController();
		if ( !ctrl ) return;

		ctrl->SetLocomotionState(locomotion);
		ctrl->RequestCommand(cmd);
	}
}

CGameScene::CGameScene()
{
    m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };
    m_localPlayerSlot = 0;

	m_grassCount = 1;
    m_groundCount = 1;
    m_villagewallCount = 1;
	m_dirtRoadCount = 1;

    m_building1Count = 1;
    m_building2Count = 1;
    m_building3Count = 1;
    m_building4Count = 1;
    m_building5Count = 1;
    m_building6Count = 1;
    m_building7Count = 1;
    m_building8Count = 1;
    m_building9Count = 1;
    m_towerCount = 1;

    m_ghoulCount = 4;
    m_swordManCount = 3;
    m_bowManCount = 3;
    m_MutantCount = 2;
    m_bossCount = 1;

    m_PlayerCount = 4;

    m_staticBatch.capacity = 0;
    m_staticBatch.count = 0;

    m_skinnedBatch.capacity = 0;
    m_skinnedBatch.count = 0;

    m_arrowRefs.clear();
    m_arrowRefs.shrink_to_fit();

	m_bulletRefs.clear();
	m_bulletRefs.shrink_to_fit();

	m_navMesh.reset();
}

#ifndef USING_NETWORK
void CGameScene::InitializeMegaGridState()
{
	for ( MegaGridCell& cell : m_megaGridCells )
	{
		cell = MegaGridCell{};
	}
}

void CGameScene::InitializeSpatialGrid()
{
	m_gridStaticCells.assign(kGridCellCount, GridStaticCell{});
	m_gridDynamicCells.assign(kGridCellCount, GridDynamicCell{});
	InitializeMegaGridState();

	for ( auto& tracker : m_playerGridTrackers )
		tracker = GridDynamicTracker{};

	m_monsterGridTrackers.clear();
	m_arrowGridTrackers.clear();
	m_bulletGridTrackers.clear();

	m_spatialGridInitialized = true;
}

void CGameScene::ShutdownSpatialGrid()
{
	m_gridStaticCells.clear();
	m_gridDynamicCells.clear();
	InitializeMegaGridState();

	for ( auto& tracker : m_playerGridTrackers )
		tracker = GridDynamicTracker{};

	m_monsterGridTrackers.clear();
	m_arrowGridTrackers.clear();
	m_bulletGridTrackers.clear();

	m_spatialGridInitialized = false;
}

bool CGameScene::WorldToGridCell(float worldX, float worldZ, int& outCellX, int& outCellZ) const
{
	if ( worldX < ( float ) kGridMinX || worldX >(float)kGridMaxX ) return false;
	if ( worldZ < ( float ) kGridMinZ || worldZ >(float)kGridMaxZ ) return false;

	int cellX = ( int ) std::floor(worldX) - kGridMinX;
	int cellZ = ( int ) std::floor(worldZ) - kGridMinZ;

	if ( cellX == kGridWidth ) cellX = kGridWidth - 1;
	if ( cellZ == kGridHeight ) cellZ = kGridHeight - 1;

	if ( cellX < 0 || cellX >= kGridWidth ) return false;
	if ( cellZ < 0 || cellZ >= kGridHeight ) return false;

	outCellX = cellX;
	outCellZ = cellZ;
	return true;
}

int CGameScene::GridCellIndex(int cellX, int cellZ) const
{
	return ( cellZ * kGridWidth ) + cellX;
}

int CGameScene::MegaGridIndex(int megaX, int megaZ) const
{
	return ( megaZ * kMegaGridCols ) + megaX;
}

bool CGameScene::FineCellToMegaGridCell(int cellX, int cellZ, int& outMegaX, int& outMegaZ) const
{
	if ( cellX < 0 || cellX >= kGridWidth ) return false;
	if ( cellZ < 0 || cellZ >= kGridHeight ) return false;

	outMegaX = cellX / kMegaGridCellWidth;
	outMegaZ = cellZ / kMegaGridCellHeight;

	if ( outMegaX < 0 || outMegaX >= kMegaGridCols ) return false;
	if ( outMegaZ < 0 || outMegaZ >= kMegaGridRows ) return false;

	return true;
}

bool CGameScene::IsFineCellInsideMegaGridApproachZone(int megaX, int megaZ, int cellX, int cellZ) const
{
	if ( megaX < 0 || megaX >= kMegaGridCols ) return false;
	if ( megaZ < 0 || megaZ >= kMegaGridRows ) return false;
	if ( cellX < 0 || cellX >= kGridWidth ) return false;
	if ( cellZ < 0 || cellZ >= kGridHeight ) return false;

	const MegaGridCell& megaCell = m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)];

	const int zoneWidth =
		std::clamp(megaCell.approachWidthCells, 1, kMegaGridCellWidth);

	const int zoneHeight =
		std::clamp(megaCell.approachHeightCells, 1, kMegaGridCellHeight);

	const int megaStartX = megaX * kMegaGridCellWidth;
	const int megaStartZ = megaZ * kMegaGridCellHeight;

	const int zoneStartX = megaStartX + ( ( kMegaGridCellWidth - zoneWidth ) / 2 );
	const int zoneStartZ = megaStartZ + ( ( kMegaGridCellHeight - zoneHeight ) / 2 );

	const int zoneEndX = zoneStartX + zoneWidth;   // exclusive
	const int zoneEndZ = zoneStartZ + zoneHeight;  // exclusive

	return
		( cellX >= zoneStartX && cellX < zoneEndX ) &&
		( cellZ >= zoneStartZ && cellZ < zoneEndZ );
}

void CGameScene::AddDynamicCount(int cellX, int cellZ, EGridDynamicKind kind, int delta)
{
	if ( !m_spatialGridInitialized ) return;
	if ( cellX < 0 || cellX >= kGridWidth ) return;
	if ( cellZ < 0 || cellZ >= kGridHeight ) return;

	GridDynamicCell& cell = m_gridDynamicCells[( size_t ) GridCellIndex(cellX, cellZ)];

	uint16_t* target = nullptr;

	switch ( kind )
	{
	case EGridDynamicKind::Player:  target = &cell.playerCount;  break;
	case EGridDynamicKind::Monster: target = &cell.monsterCount; break;
	case EGridDynamicKind::Arrow:   target = &cell.arrowCount;   break;
	case EGridDynamicKind::Bullet:  target = &cell.bulletCount;  break;
	default: return;
	}

	int nextValue = ( int ) ( *target ) + delta;
	if ( nextValue < 0 ) nextValue = 0;
	*target = ( uint16_t ) nextValue;
}

void CGameScene::StampBuildingCellsFromOOBB(const BoundingOrientedBox& box, std::unordered_set<int>& touchedCells)
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

	int beginCellX = ( int ) std::floor(minX) - kGridMinX;
	int endCellX = ( int ) std::ceil(maxX) - kGridMinX - 1;

	int beginCellZ = ( int ) std::floor(minZ) - kGridMinZ;
	int endCellZ = ( int ) std::ceil(maxZ) - kGridMinZ - 1;

	if ( beginCellX < 0 ) beginCellX = 0;
	if ( beginCellZ < 0 ) beginCellZ = 0;
	if ( endCellX >= kGridWidth ) endCellX = kGridWidth - 1;
	if ( endCellZ >= kGridHeight ) endCellZ = kGridHeight - 1;

	if ( beginCellX > endCellX ) return;
	if ( beginCellZ > endCellZ ) return;

	for ( int z = beginCellZ; z <= endCellZ; ++z )
	{
		for ( int x = beginCellX; x <= endCellX; ++x )
		{
			touchedCells.insert(GridCellIndex(x, z));
		}
	}
}

void CGameScene::RegisterStaticPlacementToGrid(const StaticPlacementEntry& placement, CGameObject* obj)
{
	if ( !m_spatialGridInitialized ) return;
	if ( !obj ) return;

	const bool isBuilding =
		( placement.assetName == "VillageWall" ) ||
		( placement.assetName == "Tower" ) ||
		( placement.assetName == "Building1" ) ||
		( placement.assetName == "Building2" ) ||
		( placement.assetName == "Building3" ) ||
		( placement.assetName == "Building4" ) ||
		( placement.assetName == "Building5" ) ||
		( placement.assetName == "Building6" ) ||
		( placement.assetName == "Building7" ) ||
		( placement.assetName == "Building8" ) ||
		( placement.assetName == "Building9" ) ||
		( placement.assetName == "Tree1" ) ||
		( placement.assetName == "Tree2" ) ||
		( placement.assetName == "Tree3" ) ||
		( placement.assetName == "Tree4" ) ||
		( placement.assetName == "Tree5" ) ||
		( placement.assetName == "Tree6" );

	if ( !isBuilding )
		return;

	std::unordered_set<int> touchedCells;

	if ( auto* collider = obj->GetComponent<CColliderComponent>() )
	{
		const std::vector<MeshOOBBSet>& meshSets = collider->GetMeshOOBBSets();

		for ( const MeshOOBBSet& set : meshSets )
		{
			for ( const BoundingOrientedBox& subOOBB : set.WorldSubOOBBs )
			{
				StampBuildingCellsFromOOBB(subOOBB, touchedCells);
			}
		}
	}

	if ( touchedCells.empty() )
	{
		int cellX = -1;
		int cellZ = -1;
		const XMFLOAT3 pos = obj->GetPosition();

		if ( WorldToGridCell(pos.x, pos.z, cellX, cellZ) )
		{
			touchedCells.insert(GridCellIndex(cellX, cellZ));
		}
	}

	for ( int cellIndex : touchedCells )
	{
		if ( cellIndex < 0 || cellIndex >= kGridCellCount ) continue;
		++m_gridStaticCells[( size_t ) cellIndex].buildingCount;
		m_gridStaticCells[( size_t ) cellIndex].floorHeight = 0.0f;
	}
}

void CGameScene::ResetDynamicGridCounts()
{
	for ( GridDynamicCell& cell : m_gridDynamicCells )
	{
		cell.playerCount = 0;
		cell.monsterCount = 0;
		cell.arrowCount = 0;
		cell.bulletCount = 0;
	}
}

bool CGameScene::TryGetTrackedCell(const CGameObject* obj, int& outCellX, int& outCellZ) const
{
	if ( !obj ) return false;

	const XMFLOAT3 pos = obj->GetPosition();
	if ( pos.y < -100.0f ) return false;

	return WorldToGridCell(pos.x, pos.z, outCellX, outCellZ);
}

void CGameScene::RefreshDynamicTracker(GridDynamicTracker& tracker, EGridDynamicKind kind)
{
	int currentCellX = -1;
	int currentCellZ = -1;
	const bool hasCurrentCell = TryGetTrackedCell(tracker.object, currentCellX, currentCellZ);

	if ( tracker.occupied )
	{
		if ( !hasCurrentCell ||
			tracker.prevCellX != currentCellX ||
			tracker.prevCellZ != currentCellZ )
		{
			AddDynamicCount(tracker.prevCellX, tracker.prevCellZ, kind, -1);
			tracker.occupied = false;
		}
	}

	if ( hasCurrentCell )
	{
		if ( !tracker.occupied ||
			tracker.prevCellX != currentCellX ||
			tracker.prevCellZ != currentCellZ )
		{
			AddDynamicCount(currentCellX, currentCellZ, kind, +1);
			tracker.prevCellX = currentCellX;
			tracker.prevCellZ = currentCellZ;
			tracker.occupied = true;
		}
	}
}

void CGameScene::BuildDynamicGridTrackers()
{
	for ( size_t i = 0; i < m_playerGridTrackers.size(); ++i )
	{
		m_playerGridTrackers[i] = GridDynamicTracker{};
		m_playerGridTrackers[i].object = m_playersBySlot[i];
	}

	m_monsterGridTrackers.clear();
	m_monsterGridTrackers.reserve(m_skinnedBatch.objectRefs.size());

	for ( CGameObject* obj : m_skinnedBatch.objectRefs )
	{
		if ( !obj ) continue;

		auto* tag = obj->GetComponent<CActorTagComponent>();
		if ( !tag ) continue;
		if ( tag->kind != EActorKind::NPC ) continue;

		GridDynamicTracker tracker{};
		tracker.object = obj;
		m_monsterGridTrackers.push_back(tracker);
	}

	m_arrowGridTrackers.clear();
	m_arrowGridTrackers.reserve(m_arrowRefs.size());

	for ( CGameObject* obj : m_arrowRefs )
	{
		GridDynamicTracker tracker{};
		tracker.object = obj;
		m_arrowGridTrackers.push_back(tracker);
	}

	m_bulletGridTrackers.clear();
	m_bulletGridTrackers.reserve(m_bulletRefs.size());

	for ( CGameObject* obj : m_bulletRefs )
	{
		GridDynamicTracker tracker{};
		tracker.object = obj;
		m_bulletGridTrackers.push_back(tracker);
	}
}

void CGameScene::RebuildDynamicGridState()
{
	if ( !m_spatialGridInitialized ) return;

	ResetDynamicGridCounts();
	BuildDynamicGridTrackers();

	for ( auto& tracker : m_playerGridTrackers )
		RefreshDynamicTracker(tracker, EGridDynamicKind::Player);

	for ( auto& tracker : m_monsterGridTrackers )
		RefreshDynamicTracker(tracker, EGridDynamicKind::Monster);

	for ( auto& tracker : m_arrowGridTrackers )
		RefreshDynamicTracker(tracker, EGridDynamicKind::Arrow);

	for ( auto& tracker : m_bulletGridTrackers )
		RefreshDynamicTracker(tracker, EGridDynamicKind::Bullet);

	UpdateMegaGridState();
}

void CGameScene::UpdateDynamicGridState()
{
	if ( !m_spatialGridInitialized ) return;

	for ( auto& tracker : m_playerGridTrackers )
		RefreshDynamicTracker(tracker, EGridDynamicKind::Player);

	for ( auto& tracker : m_monsterGridTrackers )
		RefreshDynamicTracker(tracker, EGridDynamicKind::Monster);

	for ( auto& tracker : m_arrowGridTrackers )
		RefreshDynamicTracker(tracker, EGridDynamicKind::Arrow);

	for ( auto& tracker : m_bulletGridTrackers )
		RefreshDynamicTracker(tracker, EGridDynamicKind::Bullet);

	UpdateMegaGridState();
}

void CGameScene::UpdateMegaGridState()
{
	if ( !m_spatialGridInitialized ) return;

	for ( const GridDynamicTracker& tracker : m_playerGridTrackers )
	{
		if ( !tracker.occupied ) continue;

		int megaX = -1;
		int megaZ = -1;

		if ( !FineCellToMegaGridCell(tracker.prevCellX, tracker.prevCellZ, megaX, megaZ) )
			continue;

		if ( !IsFineCellInsideMegaGridApproachZone(
			megaX,
			megaZ,
			tracker.prevCellX,
			tracker.prevCellZ) )
		{
			continue;
		}

		MegaGridCell& megaCell = m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)];

		if ( !megaCell.hasPlayerApproached )
		{
			megaCell.hasPlayerApproached = true;

			char debugText[256] = {};
			sprintf_s(
				debugText,
				"[MegaGrid] hasPlayerApproached=true | mega=(%d,%d) | playerCell=(%d,%d)\n",
				megaX,
				megaZ,
				tracker.prevCellX,
				tracker.prevCellZ
			);
			OutputDebugStringA(debugText);
		}
	}
}

void CGameScene::DumpStaticGridOccupancyLog() const
{
	if ( !m_spatialGridInitialized )
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
				m_gridStaticCells[( size_t ) GridCellIndex(x, z)];

			row.push_back(( cell.buildingCount > 0 ) ? '1' : '0');
		}

		row.push_back('\n');
		OutputDebugStringA(row.c_str());
	}

	OutputDebugStringA("[GridStatic] end\n");
}

void CGameScene::SetMegaGridApproachZoneSize(int megaX, int megaZ, int widthCells, int heightCells)
{
	if ( megaX < 0 || megaX >= kMegaGridCols ) return;
	if ( megaZ < 0 || megaZ >= kMegaGridRows ) return;

	MegaGridCell& cell = m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)];
	cell.approachWidthCells = std::clamp(widthCells, 1, kMegaGridCellWidth);
	cell.approachHeightCells = std::clamp(heightCells, 1, kMegaGridCellHeight);
}

void CGameScene::SetMegaGridCleared(int megaX, int megaZ, bool cleared)
{
	if ( megaX < 0 || megaX >= kMegaGridCols ) return;
	if ( megaZ < 0 || megaZ >= kMegaGridRows ) return;

	m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)].isCleared = cleared;
}

void CGameScene::SetMegaGridEventOccurred(int megaX, int megaZ, bool occurred)
{
	if ( megaX < 0 || megaX >= kMegaGridCols ) return;
	if ( megaZ < 0 || megaZ >= kMegaGridRows ) return;

	m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)].hasEventOccurred = occurred;
}

bool CGameScene::HasMegaGridPlayerApproached(int megaX, int megaZ) const
{
	if ( megaX < 0 || megaX >= kMegaGridCols ) return false;
	if ( megaZ < 0 || megaZ >= kMegaGridRows ) return false;

	return m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)].hasPlayerApproached;
}

bool CGameScene::IsMegaGridCleared(int megaX, int megaZ) const
{
	if ( megaX < 0 || megaX >= kMegaGridCols ) return false;
	if ( megaZ < 0 || megaZ >= kMegaGridRows ) return false;

	return m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)].isCleared;
}

bool CGameScene::HasMegaGridEventOccurred(int megaX, int megaZ) const
{
	if ( megaX < 0 || megaX >= kMegaGridCols ) return false;
	if ( megaZ < 0 || megaZ >= kMegaGridRows ) return false;

	return m_megaGridCells[( size_t ) MegaGridIndex(megaX, megaZ)].hasEventOccurred;
}
#endif

CGameScene::~CGameScene()
{
}

void CGameScene::ReleaseObjects()
{
    m_staticBatch.shader.reset();
    m_skinnedBatch.shader.reset();

	m_treeStaticShader.reset();
	m_treeAlphaClipObjects.clear();

    m_staticObjects.clear();
    m_skinnedObjects.clear();

    m_lightObjects.clear();
    m_pPlayerSpotFollower = nullptr;

    m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };

    m_staticBatch.objectRefs.clear();
    m_skinnedBatch.objectRefs.clear();

	m_colliderBatch.shader.reset();
	m_colliderBatch.objectRefs.clear();
	m_colliderObjects.clear();
	m_ColliderCount = 0;

    m_swordManRefs.clear();
    m_bowManRefs.clear();
    m_MutantRefs.clear();

	m_helmetRefs.clear();
	m_arrowRefs.clear();
	m_bulletRefs.clear();
	m_attachmentBinds.clear();
	m_staticInstanceGroups.clear();
	ResetStaticWorldLodEntries();
	m_skinnedInstanceGroups.clear();
	ResetSkinnedWorldLodEntries();

    m_PlayerSwordRefs.clear();
    m_PlayerBowRefs.clear();
    m_PlayerAxeRefs.clear();
    m_PlayerGunRefs.clear();

    m_EnemySwordRefs.clear();
    m_EnemyBowRefs.clear();

    m_preparedPlayerArrows = { nullptr, nullptr, nullptr, nullptr };
    m_prevBowReleasePhase = { false, false, false, false };
	m_preparedBowmanArrows.clear();
	m_prevEnemyBowReleasePhase.clear();

	if ( m_uiRectShader )
		m_uiRectShader->ReleaseShaderVariables();

	m_uiRectShader.reset();
	m_uiSprites.clear();
	m_pauseUISpriteIndex = -1;
	m_bInactiveOverlayVisible = false;
	m_bStartedGameplayMusic = false;
	m_bWasLocalPlayerInsideMegaGridCenter = false;

	m_navMesh.reset();

#ifndef USING_NETWORK
	m_monsterSpawnEntries.clear();
	ShutdownSpatialGrid();
#endif

	ReleaseShaderVariables();

	CScene::ReleaseObjects();
}

void CGameScene::ReleaseUploadBuffers()
{
    for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
    {
        if (!m_staticObjects[j]) continue;
        m_staticObjects[j]->ReleaseUploadBuffers();
    }
    for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
    {
        if (!m_skinnedObjects[j]) continue;
        m_skinnedObjects[j]->ReleaseUploadBuffers();
    }

#ifdef _WITH_BATCH_MATERIAL
    if (m_staticBatch.material)  m_staticBatch.material->ReleaseUploadBuffers();
#endif
}

void CGameScene::ReleaseShaderVariables()
{
	if ( m_pd3dStaticInstanceBuffer )
	{
		if ( m_pMappedStaticInstanceBuffer )
		{
			m_pd3dStaticInstanceBuffer->Unmap(0, NULL);
			m_pMappedStaticInstanceBuffer = nullptr;
		}
		m_pd3dStaticInstanceBuffer.Reset();
	}
	m_staticInstanceBufferCapacity = 0;

	if ( m_pd3dSkinnedInstanceBuffer )
	{
		if ( m_pMappedSkinnedInstanceBuffer )
		{
			m_pd3dSkinnedInstanceBuffer->Unmap(0, NULL);
			m_pMappedSkinnedInstanceBuffer = nullptr;
		}
		m_pd3dSkinnedInstanceBuffer.Reset();
	}
	m_skinnedInstanceBufferCapacity = 0;

	if ( m_pd3dSkinnedBonePaletteBuffer )
	{
		if ( m_pMappedSkinnedBonePaletteBuffer )
		{
			m_pd3dSkinnedBonePaletteBuffer->Unmap(0, NULL);
			m_pMappedSkinnedBonePaletteBuffer = nullptr;
		}
		m_pd3dSkinnedBonePaletteBuffer.Reset();
	}
	m_skinnedBonePaletteStride = 0;
	m_skinnedBonePaletteCapacity = 0;

	// ---- Static batch CB ----
	if ( m_staticBatch.cbGameObjects )
	{
		if ( m_staticBatch.mappedGameObjects )
		{
			m_staticBatch.cbGameObjects->Unmap(0, NULL);
			m_staticBatch.mappedGameObjects = nullptr;
		}
		m_staticBatch.cbGameObjects.Reset();
	}


    // ---- Skinned batch CB ----
    if (m_skinnedBatch.cbGameObjects)
    {
        if (m_skinnedBatch.mappedGameObjects)
        {
            m_skinnedBatch.cbGameObjects->Unmap(0, NULL);
            m_skinnedBatch.mappedGameObjects = nullptr;
        }
        m_skinnedBatch.cbGameObjects.Reset();
    }

    if (m_pd3dcbLights)
    {
        m_pd3dcbLights->Unmap(0, NULL);
        m_pd3dcbLights.Reset();
    }
    m_pcbMappedLights = nullptr;

    if (m_pd3dcbMaterials)
    {
        m_pd3dcbMaterials->Unmap(0, NULL);
        m_pd3dcbMaterials.Reset();
    }
    m_pcbMappedMaterials = nullptr;

	if ( m_pd3dcbFog )
	{
		if ( m_pcbMappedFog )
		{
			m_pd3dcbFog->Unmap(0, NULL);
			m_pcbMappedFog = nullptr;
		}
		m_pd3dcbFog.Reset();
	}

	if ( m_colliderBatch.cbGameObjects )
	{
		if ( m_colliderBatch.mappedGameObjects )
		{
			m_colliderBatch.cbGameObjects->Unmap(0, NULL);
			m_colliderBatch.mappedGameObjects = nullptr;
		}
		m_colliderBatch.cbGameObjects.Reset();
	}

	if ( m_uiRectShader )
	{
		m_uiRectShader->ReleaseShaderVariables();
	}
}

void CGameScene::BuildObjects(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
#ifdef USING_NETWORK
	while ( false == g_GameStarted )
	{
		OutputDebugStringA("Waiting for game start message...\n");
	}

	DequeueNetworkMessage(NetworkMessageType::GameStart);
	m_localPlayerSlot = g_myPlayerId;

	if ( !std::holds_alternative<GameStartData>(m_pendingNetworkMessage.data) )
	{
		assert(false && "Missing GameStartData in network mode");
		return;
	}

	const GameStartData& gameStartData = std::get<GameStartData>(m_pendingNetworkMessage.data);
	BuildStaticPlacementsFromNetworkGameStart(gameStartData, m_staticPlacementEntries);
	ApplyStaticPlacementCounts();
#else
	m_localPlayerSlot = 0;

	const GameSceneStageFileSet& stageFiles = GetLocalStageFileSet(kLocalStagePreset);

	if ( !LoadStaticPlacementFile(stageFiles.placementFilePath) )
	{
		assert(false && "Failed to load placement_export");
		return;
	}

	if ( !LoadSceneCubeBoxColliderReport(stageFiles.cubeColliderReportFilePath) )
	{
		assert(false && "Failed to load cube box collider report");
		return;
	}

	m_navMesh = std::make_unique<CNavMesh>();
	if ( !m_navMesh->LoadFromFile(stageFiles.navMeshFilePath) )
	{
		assert(false && "Failed to load navmesh file");
		m_navMesh.reset();
		return;
	}

	if ( !LoadMonsterSpawnFile(stageFiles.monsterSpawnFilePath) )
	{
		assert(false && "Failed to load monster spawn file");
		return;
	}
#endif

//#ifndef USING_NETWORK
//
//#else
//	m_staticPlacementEntries.clear();
//	ResetStaticPlacementCounts();
//#endif

#ifdef USING_NETWORK
	if ( std::holds_alternative<GameStartData>(m_pendingNetworkMessage.data) )
		m_PlayerCount = static_cast< UINT >( std::get<GameStartData>(m_pendingNetworkMessage.data).players.size() );
	else
		m_PlayerCount = 4;
#else
	m_PlayerCount = 4;
#endif

	m_PlayerSwordCount = m_PlayerCount;
	m_PlayerBowCount = m_PlayerCount;
	m_PlayerAxeCount = m_PlayerCount;
	m_PlayerGunCount = m_PlayerCount;

	m_helmetCount = m_MutantCount;

#ifdef USING_NETWORK
	const UINT worldStaticCount = static_cast< UINT >( m_staticPlacementEntries.size() );
#else
	const UINT worldStaticCount = static_cast< UINT >( m_staticPlacementEntries.size() );
#endif

	m_staticBatch.capacity =
		worldStaticCount +
		kArrowPoolSize +
		kBulletPoolSize +
		m_helmetCount +
		m_PlayerSwordCount +
		m_PlayerAxeCount +
		m_PlayerGunCount +
		m_swordManCount;

	m_skinnedBatch.capacity =
		m_ghoulCount +
		m_swordManCount +
		m_bowManCount +
		m_MutantCount +
		m_bossCount +
		m_PlayerCount +
		m_PlayerBowCount +
		m_bowManCount;

	m_colliderBatch.capacity = m_staticBatch.capacity + m_skinnedBatch.capacity;

	m_staticBatch.count = 0;
	m_skinnedBatch.count = 0;
	m_colliderBatch.count = 0;

	CreateGraphicsRootSignature(dev);
#ifndef USING_NETWORK
	InitializeSpatialGrid();
#endif

	auto pStaticShader = std::make_shared<CStaticObjectsShader>();
	auto pTreeStaticShader = std::make_shared<CTreeStaticObjectsShader>();
	auto pSkinnedShader = std::make_shared<CSkinnedObjectsShader>();
	auto pColliderShader = std::make_shared<CDiffusedShader>();

	m_staticBatch.shader = pStaticShader;
	m_treeStaticShader = pTreeStaticShader;
	m_skinnedBatch.shader = pSkinnedShader;
	m_colliderBatch.shader = pColliderShader;

	DXGI_FORMAT rtvFormats[5] =
	{
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R32_FLOAT
	};

	BuildLightsAndMaterials();
	BuildUIResources(dev, cmd);

	for ( auto& lo : m_lightObjects )
	{
		if ( lo ) 
			lo->CreateComponents(dev, cmd);
	}

	constexpr UINT kRTCount = 5;
	const DXGI_FORMAT kDsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

#ifndef USING_NETWORK
	//BuildColliderBatch(dev, cmd, pColliderShader, kRTCount, rtvFormats, kDsvFormat);
#endif

	pTreeStaticShader->CreateShader(dev,m_pd3dGraphicsRootSignature.Get(),kRTCount,rtvFormats,kDsvFormat);
	BuildStaticBatch(dev, cmd, pStaticShader, kRTCount, rtvFormats, kDsvFormat);
#ifndef USING_NETWORK
	//DumpStaticGridOccupancyLog();
	//BuildStaticWorldSubmeshOOBBDebugObjects(dev, cmd);
#endif
	BuildSkinnedBatch(dev, cmd, pSkinnedShader, kRTCount, rtvFormats, kDsvFormat);

	LinkSceneObjects();

	CreateShaderVariables(dev, cmd);

	CGameObject* local = GetPlayer();
	if ( !local ) 
		local = GetPlayerBySlot(0);

	CreateMainCamera(dev, cmd, local);
	BuildObjectsCollider();

#ifndef USING_NETWORK
	RebuildDynamicGridState();
#endif
#ifdef USING_NETWORK
	Protocol::C_CLIENT_READY iamReady;

	iamReady.set_ready(true);
	iamReady.set_playerid(g_myPlayerId);
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(iamReady);
	g_clientService->BroadCast(sendBuffer);
#endif
}

void CGameScene::BuildStaticWorldSubmeshOOBBDebugObjects(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd)
{
	if ( !dev || !cmd ) return;
	if ( !m_colliderBatch.mappedGameObjects ) return;

	for ( auto& ownerObj : m_staticObjects )
	{
		if ( !ownerObj ) continue;

		auto* ownerCollider = ownerObj->GetComponent<CColliderComponent>();
		if ( !ownerCollider ) continue;
		if ( ownerCollider->GetType() != EColliderType::OOBB ) continue;
		if ( ownerCollider->GetLayer() != kCollisionLayerWorldStatic ) continue;

		const std::vector<MeshOOBBSet>& meshSets = ownerCollider->GetMeshOOBBSets();
		if ( meshSets.empty() ) continue;

		for ( const MeshOOBBSet& set : meshSets )
		{
			for ( const BoundingOrientedBox& subOOBB : set.WorldSubOOBBs )
			{
				if ( m_ColliderCount >= m_colliderBatch.capacity )
				{
					OutputDebugStringA("[DebugOOBB] capacity reached\n");
					return;
				}

				const UINT i = m_ColliderCount;

				auto debugObj = std::make_unique<CGameObject>(1);

				auto* cb = reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >( m_colliderBatch.mappedGameObjects ) +
					i * m_colliderBatch.cbElementBytes
				);

				debugObj->SetMappedGameObjectCB(cb);
				debugObj->SetCbvGPUDescriptorHandlePtr(
					m_colliderBatch.baseCbvGpu.ptr + ( UINT64 ) i * m_colliderBatch.cbvInc
				);

				debugObj->AddComponent<CColliderMeshRendererComponent>();
				auto* debugCollider = debugObj->AddComponent<CColliderComponent>(EColliderType::OOBB);

				debugObj->CreateComponents(dev, cmd);

				// local unit box
				debugCollider->SetOOBB(
					XMFLOAT3(-0.5f, -0.5f, -0.5f),
					XMFLOAT3(0.5f, 0.5f, 0.5f)
				);

				// 이 월드행렬로 unit box -> subOOBB world box
				const XMFLOAT4X4 subWorld = BuildWorldMatrixFromOOBB(subOOBB);
				debugObj->SetWorldMatrix(subWorld);

				// OnUpdate는 빈 함수라 의미 없음
				debugCollider->UpdateWorldBounds();

				// 여기서 이미 world-space 꼭짓점으로 메쉬가 bake됨
				std::shared_ptr<CMesh> debugMesh =
					std::make_shared<CBoxMeshDiffused>(dev, cmd, debugCollider);

				debugObj->SetMesh(0, debugMesh);

				// bake 끝났으니 object transform은 identity로 돌려야 이중 변환이 안 생김
				debugObj->SetWorldMatrix(BuildIdentityMatrix4x4());

				CGameObject* raw = debugObj.get();
				m_colliderObjects.push_back(std::move(debugObj));
				m_colliderBatch.objectRefs.push_back(raw);
				m_colliderBatch.count = ( UINT ) m_colliderBatch.objectRefs.size();

				++m_ColliderCount;
			}
		}
	}
}

float CGameScene::QuaternionToYawDegrees(const XMFLOAT4& q)
{
    // yaw(heading) only
    const float siny_cosp = 2.0f * (q.w * q.y + q.x * q.z);
    const float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);

    const float yawRad = std::atan2(siny_cosp, cosy_cosp);
    return XMConvertToDegrees(yawRad);
}

void CGameScene::ResetStaticPlacementCounts()
{
    m_grassCount = 0;
    m_groundCount = 0;
    m_villagewallCount = 0;
    m_dirtRoadCount = 0;

    m_building1Count = 0;
    m_building2Count = 0;
    m_building3Count = 0;
    m_building4Count = 0;
    m_building5Count = 0;
    m_building6Count = 0;
    m_building7Count = 0;
    m_building8Count = 0;
    m_building9Count = 0;
    m_towerCount = 0;
}

void CGameScene::ApplyStaticPlacementCounts()
{
    ResetStaticPlacementCounts();

    for (const auto& e : m_staticPlacementEntries)
    {
        if (e.assetName == "Grass")       ++m_grassCount;
        else if (e.assetName == "Ground")      ++m_groundCount;
        else if (e.assetName == "VillageWall") ++m_villagewallCount;
        else if (e.assetName == "DirtRoad")    ++m_dirtRoadCount;
        else if (e.assetName == "Building1")   ++m_building1Count;
        else if (e.assetName == "Building2")   ++m_building2Count;
        else if (e.assetName == "Building3")   ++m_building3Count;
        else if (e.assetName == "Building4")   ++m_building4Count;
        else if (e.assetName == "Building5")   ++m_building5Count;
        else if (e.assetName == "Building6")   ++m_building6Count;
        else if (e.assetName == "Building7")   ++m_building7Count;
        else if (e.assetName == "Building8")   ++m_building8Count;
        else if (e.assetName == "Building9")   ++m_building9Count;
        else if (e.assetName == "Tower")       ++m_towerCount;
    }
}

#ifndef USING_NETWORK
bool CGameScene::LoadMonsterSpawnFile(const std::string& filePath)
{
	m_monsterSpawnEntries.clear();

	std::ifstream fin(filePath);
	if ( !fin.is_open() )
	{
		m_ghoulCount = 0;
		m_swordManCount = 0;
		m_bowManCount = 0;
		m_MutantCount = 0;
		m_bossCount = 0;
		return false;
	}

	std::string line;
	while ( std::getline(fin, line) )
	{
		if ( !line.empty() && line.back() == '\r' )
			line.pop_back();

		if ( line.rfind("SPAWN|", 0) != 0 )
			continue;

		MonsterSpawnEntry entry{};

		char type[32] = {};
		float px = 0.0f;
		float py = 0.0f;
		float pz = 0.0f;
		float yawDeg = 0.0f;
		int megaX = -1;
		int megaZ = -1;

		const int matched = sscanf_s(
			line.c_str(),
			"SPAWN|index=%d|type=\"%31[^\"]\"|mega_id=%d|mega=(%d,%d)|pos=(%f,%f,%f)|yaw_deg=%f",
			&entry.index,
			type, ( unsigned ) _countof(type),
			&entry.megaId,
			&megaX,
			&megaZ,
			&px,
			&py,
			&pz,
			&yawDeg
		);

		if ( matched != 9 )
			continue;

		entry.type = type;
		entry.megaX = megaX;
		entry.megaZ = megaZ;
		entry.pos = XMFLOAT3(px, py, pz);
		entry.yawDeg = yawDeg;

		m_monsterSpawnEntries.push_back(std::move(entry));
	}

	std::sort(
		m_monsterSpawnEntries.begin(),
		m_monsterSpawnEntries.end(),
		[ ] (const MonsterSpawnEntry& a, const MonsterSpawnEntry& b)
		{
			return a.index < b.index;
		}
	);

	ApplyMonsterSpawnCounts();
	return !m_monsterSpawnEntries.empty();
}

void CGameScene::ApplyMonsterSpawnCounts()
{
	m_ghoulCount = 0;
	m_swordManCount = 0;
	m_bowManCount = 0;
	m_MutantCount = 0;
	m_bossCount = 0;

	for ( const MonsterSpawnEntry& entry : m_monsterSpawnEntries )
	{
		if ( entry.type == "Ghoul" ) ++m_ghoulCount;
		else if ( entry.type == "SwordMan" ) ++m_swordManCount;
		else if ( entry.type == "BowMan" ) ++m_bowManCount;
		else if ( entry.type == "Mutant" ) ++m_MutantCount;
		else if ( entry.type == "Boss" ) ++m_bossCount;
	}
}
#endif

bool CGameScene::LoadStaticPlacementFile(const std::string& filePath)
{
    m_staticPlacementEntries.clear();
    ResetStaticPlacementCounts();

    std::ifstream fin(filePath);
    if (!fin.is_open())
        return false;

    std::string line;
    while (std::getline(fin, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.rfind("ENTRY|", 0) != 0)
            continue;

        StaticPlacementEntry entry{};
        if (!ParsePlacementEntryLine(line, entry))
            continue;

        entry.yawDeg = QuaternionToYawDegrees(entry.rot);
        m_staticPlacementEntries.push_back(std::move(entry));
    }

    ApplyStaticPlacementCounts();
    return !m_staticPlacementEntries.empty();
}

bool CGameScene::LoadSceneCubeBoxColliderReport(const std::string& filePath)
{
	mSceneCubeBoxColliderTable.clear();

	std::ifstream fin(filePath);
	if ( !fin.is_open() )
		return false;

	const std::string kTopRootPrefix = "TopRootName:";
	const std::string kAPathPrefix = "APath:";
	const std::string kCenterPrefix = "CenterInTopRoot_Local:";
	const std::string kRotationPrefix = "RotationInTopRoot_LocalQuat:";
	const std::string kSizePrefix = "SizeInTopRoot_Local:";

	std::string line;
	std::string currentTopRootName;
	std::string currentRelativeAPath;

	AuthoredSubMeshOOBB currentBox{};
	bool hasCenter = false;
	bool hasRotation = false;
	bool hasSize = false;

	while ( std::getline(fin, line) )
	{
		if ( !line.empty() && line.back() == '\r' )
			line.pop_back();

		const std::string trimmed = TrimString(line);

		if ( trimmed.rfind(kTopRootPrefix, 0) == 0 )
		{
			currentTopRootName = TrimString(trimmed.substr(kTopRootPrefix.size()));
			continue;
		}

		if ( trimmed.rfind(kAPathPrefix, 0) == 0 )
		{
			const std::string fullAPath = TrimString(trimmed.substr(kAPathPrefix.size()));
			currentRelativeAPath = StripTopRootFromAPath(fullAPath);
			continue;
		}

		if ( trimmed.rfind(kCenterPrefix, 0) == 0 )
		{
			hasCenter = ParseVector3Tuple(
				TrimString(trimmed.substr(kCenterPrefix.size())),
				currentBox.Center
			);
			continue;
		}

		if ( trimmed.rfind(kRotationPrefix, 0) == 0 )
		{
			hasRotation = ParseVector4Tuple(
				TrimString(trimmed.substr(kRotationPrefix.size())),
				currentBox.RotationQuat
			);
			continue;
		}

		if ( trimmed.rfind(kSizePrefix, 0) == 0 )
		{
			hasSize = ParseVector3Tuple(
				TrimString(trimmed.substr(kSizePrefix.size())),
				currentBox.Size
			);

			if ( hasCenter && hasRotation && hasSize &&
				!currentTopRootName.empty() &&
				!currentRelativeAPath.empty() )
			{
				mSceneCubeBoxColliderTable[currentTopRootName][currentRelativeAPath].push_back(currentBox);
			}

			currentBox = AuthoredSubMeshOOBB{};
			hasCenter = false;
			hasRotation = false;
			hasSize = false;
		}
	}

	return !mSceneCubeBoxColliderTable.empty();
}

bool CGameScene::ExportStaticWorldLocalOOBBReport(
	const std::string& filePath,
	const std::vector<size_t>& placementIndices,
	const std::vector<CGameObject*>& objects
) const
{
	if ( placementIndices.size() != objects.size() )
		return false;

	std::ofstream fout(filePath, std::ios::out | std::ios::trunc);
	if ( !fout.is_open() )
		return false;

	fout << std::fixed << std::setprecision(6);

	auto WriteFloat3 = [ &fout ] (const char* label, const XMFLOAT3& v)
		{
			fout
				<< label
				<< "(" << v.x << ", " << v.y << ", " << v.z << ")\n";
		};

	auto WriteFloat4 = [ &fout ] (const char* label, const XMFLOAT4& v)
		{
			fout
				<< label
				<< "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")\n";
		};

	auto WriteOOBB = [ & ] (const std::string& prefix, const BoundingOrientedBox& box)
		{
			fout
				<< prefix
				<< "Center: ("
				<< box.Center.x << ", "
				<< box.Center.y << ", "
				<< box.Center.z << ")\n";

			fout
				<< prefix
				<< "RotationQuat: ("
				<< box.Orientation.x << ", "
				<< box.Orientation.y << ", "
				<< box.Orientation.z << ", "
				<< box.Orientation.w << ")\n";

			fout
				<< prefix
				<< "Extents: ("
				<< box.Extents.x << ", "
				<< box.Extents.y << ", "
				<< box.Extents.z << ")\n";

			fout
				<< prefix
				<< "Size: ("
				<< box.Extents.x * 2.0f << ", "
				<< box.Extents.y * 2.0f << ", "
				<< box.Extents.z * 2.0f << ")\n";
		};

	fout << "StaticWorldLocalOOBBReportBegin\n";
	fout << "ObjectCount: " << objects.size() << "\n\n";

	for ( size_t i = 0; i < objects.size(); ++i )
	{
		const size_t placementIndex = placementIndices[i];
		if ( placementIndex >= m_staticPlacementEntries.size() )
			continue;

		CGameObject* obj = objects[i];
		if ( !obj )
			continue;

		auto* collider = obj->GetComponent<CColliderComponent>();
		if ( !collider )
			continue;

		const StaticPlacementEntry& placement = m_staticPlacementEntries[placementIndex];
		const BoundingOrientedBox& objectLocalOOBB = collider->GetLocalOOBB();
		const std::vector<MeshOOBBSet>& meshSets = collider->GetMeshOOBBSets();

		fout << "ObjectBegin\n";
		fout << "PlacementIndex: " << placementIndex << "\n";
		fout << "AssetName: " << placement.assetName << "\n";
		fout << "ObjectName: " << placement.objectName << "\n";
		WriteFloat3("ObjectWorldPosition: ", placement.pos);
		WriteFloat4("ObjectWorldRotationQuat: ", placement.rot);
		fout << "ObjectWorldYawDeg: " << placement.yawDeg << "\n";

		WriteOOBB("OverallLocalOOBB.", objectLocalOOBB);

		fout << "MeshOOBBSetCount: " << meshSets.size() << "\n";

		for ( size_t meshIndex = 0; meshIndex < meshSets.size(); ++meshIndex )
		{
			const MeshOOBBSet& set = meshSets[meshIndex];

			fout << "MeshSetBegin\n";
			fout << "MeshIndex: " << meshIndex << "\n";

			WriteOOBB("MeshLocalOOBB.", set.LocalMeshOOBB);

			fout << "SubOOBBCount: " << set.LocalSubOOBBs.size() << "\n";

			for ( size_t subIndex = 0; subIndex < set.LocalSubOOBBs.size(); ++subIndex )
			{
				fout << "SubOOBBBegin\n";
				fout << "SubIndex: " << subIndex << "\n";
				WriteOOBB("SubLocalOOBB.", set.LocalSubOOBBs[subIndex]);
				fout << "SubOOBBEnd\n";
			}

			fout << "MeshSetEnd\n";
		}

		fout << "ObjectEnd\n\n";
	}

	fout << "StaticWorldLocalOOBBReportEnd\n";
	return true;
}

void CGameScene::BuildLightsAndMaterials()
{
    m_lightObjects.clear();
    m_lightObjects.reserve(4);
    m_pPlayerSpotFollower = nullptr;

    // [0] Point Light
    {
        auto obj = std::make_unique<CGameObject>(0);
        obj->SetPosition(1.0f, 0.0f, 0.0f);

        auto* lc = obj->AddComponent<CLightComponent>();
        lc->type = ELightType::Point;
        lc->range = 100.0f;
        lc->ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        lc->diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        lc->specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        lc->attenuation = XMFLOAT3(1.0f, 0.001f, 0.0001f);

        m_lightObjects.push_back(std::move(obj));
    }

    // [1] Spot Light (player follow)
    {
        auto obj = std::make_unique<CGameObject>(0);

        auto* lc = obj->AddComponent<CLightComponent>();
        lc->type = ELightType::Spot;
        lc->range = 50.0f;
        lc->ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        lc->diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        lc->specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        lc->attenuation = XMFLOAT3(1.0f, 0.01f, 0.0001f);
        lc->falloff = 8.0f;
        lc->cosPhi = (float)cos(XMConvertToRadians(40.0f));
        lc->cosTheta = (float)cos(XMConvertToRadians(20.0f));

        auto* follow = obj->AddComponent<CFollowTransformComponent>();
        m_pPlayerSpotFollower = follow;

        m_lightObjects.push_back(std::move(obj));
    }

	// [2] Directional Light
	{
		auto obj = std::make_unique<CGameObject>(0);

		if ( auto* tr = obj->GetComponent<CTransformComponent>() )
		{
			// 오른쪽 위 앞쪽에서 왼쪽 아래 뒤쪽으로 비추는 느낌
			tr->SetLookDirection(XMFLOAT3(1.0f, -1.0f, 0.3f));
		}

		auto* lc = obj->AddComponent<CLightComponent>();
		lc->type = ELightType::Directional;

		lc->ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		lc->diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		lc->specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		m_lightObjects.push_back(std::move(obj));
	}

    // [3] Spot Light
    {
        auto obj = std::make_unique<CGameObject>(0);
        obj->SetPosition(-150.0f, 30.0f, 30.0f);
        if (auto* tr = obj->GetComponent<CTransformComponent>())
            tr->SetLookDirection(XMFLOAT3(0.0f, 1.0f, 1.0f));

        auto* lc = obj->AddComponent<CLightComponent>();
        lc->type = ELightType::Spot;
        lc->range = 60.0f;
        lc->ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        lc->diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        lc->specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        lc->attenuation = XMFLOAT3(1.0f, 0.01f, 0.0001f);
        lc->falloff = 8.0f;
        lc->cosPhi = (float)cos(XMConvertToRadians(90.0f));
        lc->cosTheta = (float)cos(XMConvertToRadians(30.0f));

        m_lightObjects.push_back(std::move(obj));
    }

    m_pMaterials = make_unique<MATERIALS>();
    ::ZeroMemory(m_pMaterials.get(), sizeof(MATERIALS));

    m_pMaterials->m_pReflections[0] = {
        XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 5.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[1] = {
        XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f),
        XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 10.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[2] = {
        XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 15.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[3] = {
        XMFLOAT4(0.5f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(0.0f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 20.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[4] = {
        XMFLOAT4(0.0f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(0.5f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 25.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[5] = {
        XMFLOAT4(0.0f, 0.5f, 0.5f, 1.0f),
        XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 30.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[6] = {
        XMFLOAT4(0.5f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(0.5f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 35.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[7] = {
        XMFLOAT4(1.0f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 40.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    for (int i = 0; i < MAX_MATERIALS; ++i)
        m_pMaterials->m_pReflections[i].m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);
}

void CGameScene::CreateShaderVariables(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
    UINT ncbElementBytes = ((sizeof(LIGHTS) + 255) & ~255);
    m_pd3dcbLights = ::CreateBufferResource(
        dev, cmd, nullptr,
        ncbElementBytes,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        nullptr);
    m_pd3dcbLights->Map(0, nullptr, (void**)&m_pcbMappedLights);

    UINT ncbMaterialBytes = ((sizeof(MATERIALS) + 255) & ~255);
    m_pd3dcbMaterials = ::CreateBufferResource(
        dev, cmd, nullptr,
        ncbMaterialBytes,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        nullptr);
    m_pd3dcbMaterials->Map(0, nullptr, (void**)&m_pcbMappedMaterials);

	UINT ncbFogBytes = ( ( sizeof(CB_FOG) + 255 ) & ~255 );
	m_pd3dcbFog = ::CreateBufferResource(
		dev, cmd, nullptr,
		ncbFogBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr);
	m_pd3dcbFog->Map(0, nullptr, ( void** ) &m_pcbMappedFog);
}

void CGameScene::BuildStaticBatch(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	const std::shared_ptr<CStaticObjectsShader>& pStaticShader,
	UINT nRenderTargets,
	DXGI_FORMAT* rtvFormats,
	DXGI_FORMAT dsvFormat)
{
	auto* b = &m_staticBatch;
	if ( !b ) return;

	if ( b->capacity < 4 ) b->capacity = 4;
	const UINT cap = b->capacity;
	if ( cap == 0 ) return;

	pStaticShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		nRenderTargets,
		rtvFormats,
		dsvFormat
	);

	b->cbElementBytes = ( ( sizeof(CB_GAMEOBJECT_INFO) + 255 ) & ~255 );

	b->cbGameObjects = ::CreateBufferResource(
		dev, cmd, nullptr,
		b->cbElementBytes * cap,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	b->cbGameObjects->Map(0, nullptr, ( void** ) &b->mappedGameObjects);

	b->baseCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
	b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	m_pDescriptorHeap->CreateConstantBufferViews(
		dev,
		cap,
		b->cbGameObjects.Get(),
		b->cbElementBytes
	);

	m_treeAlphaClipObjects.clear();

	m_staticObjects.clear();
	m_staticObjects.reserve(cap);

	b->objectRefs.clear();
	b->objectRefs.reserve(cap);

	b->count = 0;
	ResetStaticWorldLodEntries();

	std::vector<size_t> exportedWorldStaticPlacementIndices;
	std::vector<CGameObject*> exportedWorldStaticObjects;

	exportedWorldStaticPlacementIndices.reserve(m_staticPlacementEntries.size());
	exportedWorldStaticObjects.reserve(m_staticPlacementEntries.size());

	auto MakeStaticContext = [ & ] (UINT objectIndex)
		{
			GameSceneObjectFactory::CreateContext ctx{};
			ctx.device = dev;
			ctx.cmd = cmd;
			ctx.mappedGameObjectCB =
				reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >( b->mappedGameObjects ) +
					objectIndex * b->cbElementBytes
				);
			ctx.cbvGpuHandle.ptr =
				b->baseCbvGpu.ptr + ( UINT64 ) objectIndex * b->cbvInc;
			return ctx;
		};

	// ------------------------------------------------------------------------
	// Static world objects from placement file
	// ------------------------------------------------------------------------
	for ( UINT k = 0; k < ( UINT ) m_staticPlacementEntries.size(); ++k )
	{
		if ( b->objectRefs.size() >= b->capacity ) break;

		const UINT i = ( UINT ) b->objectRefs.size();
		const StaticPlacementEntry& placement = m_staticPlacementEntries[k];

		const bool createWorldStaticCollider =
			ShouldCreateWorldStaticCollider(placement.assetName);
		const bool isStaticWorldLodTarget =
			IsStaticWorldLodSupportedAssetName(placement.assetName);
		const bool enableDistanceCull =
			ShouldUseStaticWorldDistanceCull(placement.assetName);

		AssetBuildDesc desc{};
		AssetType resolvedAssetType{};
		if ( !ResolveStaticAssetDesc(placement.assetName, desc, &resolvedAssetType) )
			continue;

		std::shared_ptr<CMesh> selectedMesh = nullptr;
		std::array<std::shared_ptr<CMesh>, 3> loadedLodMeshes = { nullptr, nullptr, nullptr };
		bool enableStaticWorldLod = false;

		if ( isStaticWorldLodTarget )
		{
			enableStaticWorldLod = true;

			for ( int lodLevel = 0; lodLevel < 3; ++lodLevel )
			{
				AssetBuildDesc lodDesc{};
				AssetType lodResolvedType{};
				if ( !ResolveStaticLodAssetDesc(
					placement.assetName,
					lodLevel,
					lodDesc,
					&lodResolvedType) )
				{
					enableStaticWorldLod = false;
					break;
				}

				BuiltAsset lodAsset = AssetManager::BuildAsset(
					dev, cmd,
					m_pMaterials.get(),
					lodDesc
				);

				loadedLodMeshes[( size_t ) lodLevel] = lodAsset.mesh;
				if ( !loadedLodMeshes[( size_t ) lodLevel] )
				{
					enableStaticWorldLod = false;
					break;
				}
			}

			if ( enableStaticWorldLod )
			{
				selectedMesh = loadedLodMeshes[0];
			}
			else
			{
				loadedLodMeshes = { nullptr, nullptr, nullptr };
			}
		}

		if ( !selectedMesh )
		{
			BuiltAsset baseAsset = AssetManager::BuildAsset(
				dev, cmd,
				m_pMaterials.get(),
				desc
			);

			selectedMesh = baseAsset.mesh;
			loadedLodMeshes[0] = selectedMesh;
		}

		if ( !selectedMesh )
			continue;

		GameSceneObjectFactory::StaticRenderableDesc createDesc{};
		createDesc.ctx = MakeStaticContext(i);
		createDesc.mesh = selectedMesh;
		createDesc.position = placement.pos;
		createDesc.yawDeg = placement.yawDeg;

		createDesc.addCollider = createWorldStaticCollider;
		createDesc.colliderType = EColliderType::OOBB;
		createDesc.colliderLayer = kCollisionLayerWorldStatic;
		createDesc.colliderMask = CollisionBit(kCollisionLayerPlayer);

		if ( createWorldStaticCollider )
		{
			const auto authoredIt = mSceneCubeBoxColliderTable.find(placement.assetName);
			if ( authoredIt != mSceneCubeBoxColliderTable.end() )
			{
				createDesc.authoredStaticSubMeshOOBBs = &authoredIt->second;
			}
		}

		auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
		if ( !obj )
			continue;

		CGameObject* raw = obj.get();

		if ( resolvedAssetType == AssetType::Tree )
		{
			m_treeAlphaClipObjects.insert(raw);
		}

		if ( enableDistanceCull || isStaticWorldLodTarget )
		{
			StaticWorldLodEntry lodEntry{};
			lodEntry.object = raw;
			lodEntry.staticBatchObjectIndex = i;
			lodEntry.assetName = placement.assetName;
			lodEntry.lodReferencePosition = placement.pos;

			lodEntry.lodEnabled = ( isStaticWorldLodTarget && enableStaticWorldLod );
			lodEntry.useTreeShader = ( resolvedAssetType == AssetType::Tree );
			lodEntry.currentLod = 0;
			lodEntry.lodMeshes = loadedLodMeshes;

			lodEntry.distanceCullEnabled = enableDistanceCull;
			lodEntry.distanceCulled = false;

			if ( placement.assetName == "VillageWall" )
			{
				lodEntry.lodDistance01 = 250.0f;
				lodEntry.lodDistance12 = 600.0f;
				lodEntry.cullDistance = 900.0f;
			}
			else if (
				placement.assetName == "Building1" ||
				placement.assetName == "Building2" ||
				placement.assetName == "Building3" ||
				placement.assetName == "Building4" ||
				placement.assetName == "Building5" ||
				placement.assetName == "Building6" ||
				placement.assetName == "Building7" ||
				placement.assetName == "Building8" ||
				placement.assetName == "Building9" ||
				placement.assetName == "Tower" )
			{
				lodEntry.lodDistance01 = 150.0f;
				lodEntry.lodDistance12 = 380.0f;
				lodEntry.cullDistance = 550.0f;
			}
			else if (
				placement.assetName == "Tree1" ||
				placement.assetName == "Tree2" ||
				placement.assetName == "Tree3" ||
				placement.assetName == "Tree4" ||
				placement.assetName == "Tree5" ||
				placement.assetName == "Tree6" )
			{
				lodEntry.lodDistance01 = 40.0f;
				lodEntry.lodDistance12 = 120.0f;
				lodEntry.cullDistance = 250.0f;
			}
			else
			{
				lodEntry.cullDistance = 400.0f;
			}

			if ( !lodEntry.lodMeshes[0] )
				lodEntry.lodMeshes[0] = selectedMesh;

			m_staticWorldLodEntries.push_back(std::move(lodEntry));
		}

		if ( createWorldStaticCollider )
		{
			auto* collider = raw->GetComponent<CColliderComponent>();
			if ( collider && collider->GetType() == EColliderType::OOBB )
			{
				exportedWorldStaticPlacementIndices.push_back(static_cast< size_t >( k ));
				exportedWorldStaticObjects.push_back(raw);
			}
		}

#ifndef USING_NETWORK
		RegisterStaticPlacementToGrid(placement, raw);
#endif

		m_staticObjects.push_back(std::move(obj));
		b->objectRefs.push_back(raw);
		b->count = ( UINT ) b->objectRefs.size();
	}

#ifndef USING_NETWORK
	if ( kEnableStaticWorldLocalOOBBReportExport )
	{
		if ( !ExportStaticWorldLocalOOBBReport(
			kStaticWorldLocalOOBBReportPath,
			exportedWorldStaticPlacementIndices,
			exportedWorldStaticObjects) )
		{
			OutputDebugStringA("[StaticWorldLocalOOBBReport] export failed\n");
		}
		else
		{
			OutputDebugStringA("[StaticWorldLocalOOBBReport] export complete\n");
		}
	}
#endif

	// ------------------------------------------------------------------------
	// Arrow pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc arrowDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::Arrow, arrowDesc);

		BuiltAsset arrowAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			arrowDesc
		);

		m_arrowRefs.clear();
		m_arrowRefs.reserve(kArrowPoolSize);

		for ( UINT k = 0; k < kArrowPoolSize; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = arrowAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addCollider = true;
			createDesc.colliderType = EColliderType::OOBB;
			createDesc.colliderLayer = kCollisionLayerPlayerWeapon;
			createDesc.colliderMask = CollisionBit(kCollisionLayerMonster);
			createDesc.colliderEnabled = false;

			createDesc.addArrowComponent = true;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_arrowRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// Bullet pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc bulletDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::Bullet, bulletDesc);

		BuiltAsset bulletAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			bulletDesc
		);

		m_bulletRefs.clear();
		m_bulletRefs.reserve(kBulletPoolSize);

		for ( UINT k = 0; k < kBulletPoolSize; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = bulletAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addCollider = true;
			createDesc.colliderType = EColliderType::BSphere;
			createDesc.colliderLayer = kCollisionLayerPlayerWeapon;
			createDesc.colliderMask = CollisionBit(kCollisionLayerMonster);
			createDesc.colliderEnabled = false;

			createDesc.addBulletComponent = true;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_bulletRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// Helmet pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc helmetDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::Helmet, helmetDesc);

		BuiltAsset helmetAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			helmetDesc
		);

		m_helmetRefs.clear();
		m_helmetRefs.reserve(m_helmetCount);

		for ( UINT k = 0; k < m_helmetCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = helmetAsset.mesh;
			createDesc.spawnHidden = true;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_helmetRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// PlayerSword pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc swordDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerSword, swordDesc);

		BuiltAsset swordAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			swordDesc
		);

		m_PlayerSwordRefs.clear();
		m_PlayerSwordRefs.reserve(m_PlayerSwordCount);

		for ( UINT k = 0; k < m_PlayerSwordCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = swordAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addCollider = true;
			createDesc.colliderType = EColliderType::OOBB;
			createDesc.colliderLayer = kCollisionLayerPlayerWeapon;
			createDesc.colliderMask = CollisionBit(kCollisionLayerMonster);
			createDesc.colliderEnabled = false;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_PlayerSwordRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// PlayerAxe pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc axeDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerAxe, axeDesc);

		BuiltAsset axeAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			axeDesc
		);

		m_PlayerAxeRefs.clear();
		m_PlayerAxeRefs.reserve(m_PlayerAxeCount);

		for ( UINT k = 0; k < m_PlayerAxeCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = axeAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addCollider = true;
			createDesc.colliderType = EColliderType::OOBB;
			createDesc.colliderLayer = kCollisionLayerPlayerWeapon;
			createDesc.colliderMask = CollisionBit(kCollisionLayerMonster);
			createDesc.colliderEnabled = false;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_PlayerAxeRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// PlayerGun pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc gunDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerGun, gunDesc);

		BuiltAsset gunAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			gunDesc
		);

		m_PlayerGunRefs.clear();
		m_PlayerGunRefs.reserve(m_PlayerGunCount);

		for ( UINT k = 0; k < m_PlayerGunCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = gunAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addCollider = true;
			createDesc.colliderType = EColliderType::OOBB;
			createDesc.configureColliderFiltering = false;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_PlayerGunRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// EnemySword pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc enemySwordDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::EnemySword, enemySwordDesc);

		BuiltAsset swordAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			enemySwordDesc
		);

		m_EnemySwordRefs.clear();
		m_EnemySwordRefs.reserve(m_swordManCount);

		for ( UINT k = 0; k < m_swordManCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = swordAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addCollider = true;
			createDesc.colliderType = EColliderType::OOBB;
			createDesc.colliderLayer = kCollisionLayerMonsterWeapon;
			createDesc.colliderMask = CollisionBit(kCollisionLayerPlayer);
			createDesc.colliderEnabled = false;

			createDesc.addMonsterWeaponHitbox = true;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_EnemySwordRefs.push_back(raw);
		}
	}

	BuildStaticInstanceGroups();

	if ( m_pd3dStaticInstanceBuffer )
	{
		if ( m_pMappedStaticInstanceBuffer )
		{
			m_pd3dStaticInstanceBuffer->Unmap(0, NULL);
			m_pMappedStaticInstanceBuffer = nullptr;
		}
		m_pd3dStaticInstanceBuffer.Reset();
	}

	if ( m_staticInstanceBufferCapacity > 0 )
	{
		const UINT instanceBufferBytes =
			sizeof(StaticInstanceVertex) * m_staticInstanceBufferCapacity;

		m_pd3dStaticInstanceBuffer = ::CreateBufferResource(
			dev, cmd, nullptr,
			instanceBufferBytes,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr
		);

		m_pd3dStaticInstanceBuffer->Map(
			0, nullptr, ( void** ) &m_pMappedStaticInstanceBuffer);
	}
}

void CGameScene::ResetStaticWorldLodEntries()
{
	m_staticWorldLodEntries.clear();
	m_staticDistanceCullFlags.clear();
	m_staticWorldLodDirty = false;
}

int CGameScene::ComputeStaticWorldLodLevel(const XMFLOAT3& cameraPosition, const StaticWorldLodEntry& entry) const
{
	if ( !entry.lodEnabled )
		return 0;

	const float dx = cameraPosition.x - entry.lodReferencePosition.x;
	const float dy = cameraPosition.y - entry.lodReferencePosition.y;
	const float dz = cameraPosition.z - entry.lodReferencePosition.z;

	const float distSq = dx * dx + dy * dy + dz * dz;
	const float dist = std::sqrt(distSq);

	const float lodDistance01 = std::max(0.0f, entry.lodDistance01);
	const float lodDistance12 = std::max(lodDistance01, entry.lodDistance12);

	const float lod01Enter = lodDistance01 + m_staticLodHysteresis;
	const float lod01Exit = lodDistance01 - m_staticLodHysteresis;
	const float lod12Enter = lodDistance12 + m_staticLodHysteresis;
	const float lod12Exit = lodDistance12 - m_staticLodHysteresis;

	switch ( entry.currentLod )
	{
	case 0:
		if ( dist >= lod01Enter ) return 1;
		return 0;

	case 1:
		if ( dist < lod01Exit ) return 0;
		if ( dist >= lod12Enter ) return 2;
		return 1;

	case 2:
		if ( dist < lod12Exit ) return 1;
		return 2;

	default:
		break;
	}

	if ( dist < lodDistance01 ) return 0;
	if ( dist < lodDistance12 ) return 1;
	return 2;
}

bool CGameScene::ComputeStaticWorldDistanceCulled(
	const XMFLOAT3& cameraPosition,
	const StaticWorldLodEntry& entry) const
{
	if ( !entry.distanceCullEnabled )
		return false;

	const float dx = cameraPosition.x - entry.lodReferencePosition.x;
	const float dy = cameraPosition.y - entry.lodReferencePosition.y;
	const float dz = cameraPosition.z - entry.lodReferencePosition.z;

	const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

	const float cullDistance = std::max(0.0f, entry.cullDistance);
	const float cullEnter = cullDistance + m_staticCullHysteresis;
	const float cullExit = std::max(0.0f, cullDistance - m_staticCullHysteresis);

	if ( !entry.distanceCulled )
	{
		if ( dist >= cullEnter ) return true;
		return false;
	}

	if ( dist < cullExit ) return false;
	return true;
}

void CGameScene::UpdateStaticWorldLodSelection(CCamera* camera)
{
	if ( !camera )
	{
		m_staticDistanceCullFlags.clear();
		return;
	}

	m_staticDistanceCullFlags.assign(m_staticBatch.objectRefs.size(), 0);

	if ( m_staticWorldLodEntries.empty() )
	{
		m_staticWorldLodDirty = false;
		return;
	}

	const XMFLOAT3 cameraPosition = camera->GetPosition();

	for ( StaticWorldLodEntry& entry : m_staticWorldLodEntries )
	{
		if ( !entry.object ) continue;
		if ( entry.staticBatchObjectIndex == UINT_MAX ) continue;
		if ( entry.staticBatchObjectIndex >= ( UINT ) m_staticDistanceCullFlags.size() ) continue;

		const bool distanceCulled =
			ComputeStaticWorldDistanceCulled(cameraPosition, entry);

		entry.distanceCulled = distanceCulled;

		if ( distanceCulled )
			m_staticDistanceCullFlags[entry.staticBatchObjectIndex] = 1;
	}

	m_staticWorldLodDirty = false;
}

void CGameScene::ResetSkinnedWorldLodEntries()
{
	m_skinnedWorldLodEntries.clear();
	m_skinnedDistanceCullFlags.clear();
	m_skinnedWorldLodDirty = false;
}

int CGameScene::ComputeSkinnedWorldLodLevel(const XMFLOAT3& cameraPosition, const SkinnedWorldLodEntry& entry) const
{
	if ( !entry.lodEnabled )
		return 0;

	const float dx = cameraPosition.x - entry.lodReferencePosition.x;
	const float dy = cameraPosition.y - entry.lodReferencePosition.y;
	const float dz = cameraPosition.z - entry.lodReferencePosition.z;

	const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

	const float lodDistance01 = std::max(0.0f, entry.lodDistance01);
	const float lodDistance12 = std::max(lodDistance01, entry.lodDistance12);

	const float lod01Enter = lodDistance01 + m_skinnedLodHysteresis;
	const float lod01Exit = lodDistance01 - m_skinnedLodHysteresis;
	const float lod12Enter = lodDistance12 + m_skinnedLodHysteresis;
	const float lod12Exit = lodDistance12 - m_skinnedLodHysteresis;

	switch ( entry.currentLod )
	{
	case 0:
		if ( dist >= lod01Enter ) return 1;
		return 0;

	case 1:
		if ( dist < lod01Exit ) return 0;
		if ( dist >= lod12Enter ) return 2;
		return 1;

	case 2:
		if ( dist < lod12Exit ) return 1;
		return 2;
	}

	if ( dist < lodDistance01 ) return 0;
	if ( dist < lodDistance12 ) return 1;
	return 2;
}

bool CGameScene::ComputeSkinnedWorldDistanceCulled(
	const XMFLOAT3& cameraPosition,
	const SkinnedWorldLodEntry& entry) const
{
	if ( !entry.distanceCullEnabled )
		return false;

	const float dx = cameraPosition.x - entry.lodReferencePosition.x;
	const float dy = cameraPosition.y - entry.lodReferencePosition.y;
	const float dz = cameraPosition.z - entry.lodReferencePosition.z;

	const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

	const float cullDistance = std::max(0.0f, entry.cullDistance);
	const float cullEnter = cullDistance + m_skinnedCullHysteresis;
	const float cullExit = std::max(0.0f, cullDistance - m_skinnedCullHysteresis);

	if ( !entry.distanceCulled )
	{
		if ( dist >= cullEnter ) return true;
		return false;
	}

	if ( dist < cullExit ) return false;
	return true;
}

void CGameScene::UpdateSkinnedWorldLodSelection(CCamera* camera)
{
	if ( !camera )
	{
		m_skinnedDistanceCullFlags.clear();
		return;
	}

	m_skinnedDistanceCullFlags.assign(m_skinnedBatch.objectRefs.size(), 0);

	if ( m_skinnedWorldLodEntries.empty() )
	{
		m_skinnedWorldLodDirty = false;
		return;
	}

	const XMFLOAT3 cameraPosition = camera->GetPosition();
	bool anyLodChanged = false;

	for ( SkinnedWorldLodEntry& entry : m_skinnedWorldLodEntries )
	{
		if ( !entry.object ) continue;
		if ( entry.skinnedBatchObjectIndex == UINT_MAX ) continue;
		if ( entry.skinnedBatchObjectIndex >= ( UINT ) m_skinnedDistanceCullFlags.size() ) continue;

		const bool distanceCulled =
			ComputeSkinnedWorldDistanceCulled(cameraPosition, entry);

		entry.distanceCulled = distanceCulled;

		if ( distanceCulled )
		{
			m_skinnedDistanceCullFlags[entry.skinnedBatchObjectIndex] = 1;
			continue;
		}

		int desiredLod = ComputeSkinnedWorldLodLevel(cameraPosition, entry);
		desiredLod = ClampSkinnedWorldLodLevel(desiredLod);

		int resolvedLod = desiredLod;
		while ( resolvedLod > 0 && !entry.lodMeshes[( size_t ) resolvedLod] )
			--resolvedLod;

		std::shared_ptr<CMesh> targetMesh = entry.lodMeshes[( size_t ) resolvedLod];
		if ( !targetMesh ) continue;

		std::shared_ptr<CMesh> currentMesh = entry.object->GetMeshShared(0);
		if ( entry.currentLod == resolvedLod && currentMesh.get() == targetMesh.get() )
			continue;
	}

	// --------------------------------------------------------------------
	// body가 culled 되면 attachment follower도 같이 culled 처리
	// - static follower : sword, helmet 등
	// - skinned follower: bow 등
	// --------------------------------------------------------------------
	std::unordered_map<const CGameObject*, UINT> staticIndexByObject;
	staticIndexByObject.reserve(m_staticBatch.objectRefs.size());

	for ( UINT i = 0; i < ( UINT ) m_staticBatch.objectRefs.size(); ++i )
	{
		if ( m_staticBatch.objectRefs[i] )
			staticIndexByObject[m_staticBatch.objectRefs[i]] = i;
	}

	std::unordered_map<const CGameObject*, UINT> skinnedIndexByObject;
	skinnedIndexByObject.reserve(m_skinnedBatch.objectRefs.size());

	for ( UINT i = 0; i < ( UINT ) m_skinnedBatch.objectRefs.size(); ++i )
	{
		if ( m_skinnedBatch.objectRefs[i] )
			skinnedIndexByObject[m_skinnedBatch.objectRefs[i]] = i;
	}

	for ( const AttachmentBindSpec& spec : m_attachmentBinds )
	{
		if ( !spec.follower || !spec.target )
			continue;

		auto targetIt = skinnedIndexByObject.find(spec.target);
		if ( targetIt == skinnedIndexByObject.end() )
			continue;

		const UINT targetIndex = targetIt->second;
		if ( targetIndex >= ( UINT ) m_skinnedDistanceCullFlags.size() )
			continue;

		if ( m_skinnedDistanceCullFlags[targetIndex] == 0 )
			continue;

		auto followerStaticIt = staticIndexByObject.find(spec.follower);
		if ( followerStaticIt != staticIndexByObject.end() )
		{
			const UINT followerIndex = followerStaticIt->second;
			if ( followerIndex < ( UINT ) m_staticDistanceCullFlags.size() )
				m_staticDistanceCullFlags[followerIndex] = 1;
		}

		auto followerSkinnedIt = skinnedIndexByObject.find(spec.follower);
		if ( followerSkinnedIt != skinnedIndexByObject.end() )
		{
			const UINT followerIndex = followerSkinnedIt->second;
			if ( followerIndex < ( UINT ) m_skinnedDistanceCullFlags.size() )
				m_skinnedDistanceCullFlags[followerIndex] = 1;
		}
	}

	if ( anyLodChanged )
	{
		BuildSkinnedInstanceGroups();
		m_skinnedWorldLodDirty = true;
	}
	else
	{
		m_skinnedWorldLodDirty = false;
	}
}

void CGameScene::BuildStaticInstanceGroups()
{
	m_staticInstanceGroups.clear();

	for ( UINT objectIndex = 0; objectIndex < ( UINT ) m_staticBatch.objectRefs.size(); ++objectIndex )
	{
		CGameObject* obj = m_staticBatch.objectRefs[objectIndex];
		const bool useTreeShader =
			( m_treeAlphaClipObjects.find(obj) != m_treeAlphaClipObjects.end() );
		if ( !obj ) continue;

		const int meshCount = obj->GetMeshCount();
		for ( int meshIndex = 0; meshIndex < meshCount; ++meshIndex )
		{
			std::shared_ptr<CMesh> mesh = obj->GetMeshShared(meshIndex);
			if ( !mesh ) continue;

			for ( UINT subMeshIndex = 0; subMeshIndex < ( UINT ) mesh->m_SubMeshes.size(); ++subMeshIndex )
			{
				StaticInstanceGroup* targetGroup = nullptr;

				for ( StaticInstanceGroup& group : m_staticInstanceGroups )
				{
					if ( group.mesh.get() == mesh.get() &&
						group.subMeshIndex == subMeshIndex &&
						group.useTreeShader == useTreeShader )
					{
						targetGroup = &group;
						break;
					}
				}

				if ( !targetGroup )
				{
					StaticInstanceGroup newGroup{};
					newGroup.mesh = mesh;
					newGroup.subMeshIndex = subMeshIndex;
					newGroup.useTreeShader = useTreeShader;
					m_staticInstanceGroups.push_back(std::move(newGroup));
					targetGroup = &m_staticInstanceGroups.back();
				}

				targetGroup->objectIndices.push_back(objectIndex);
			}
		}
	}

	UINT runningStart = 0;
	for ( StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		group.instanceBufferStart = runningStart;
		runningStart += ( UINT ) group.objectIndices.size();
	}

	m_staticInstanceBufferCapacity = runningStart;
}

void CGameScene::BuildSkinnedInstanceGroups()
{
	m_skinnedInstanceGroups.clear();

	for ( UINT objectIndex = 0; objectIndex < ( UINT ) m_skinnedBatch.objectRefs.size(); ++objectIndex )
	{
		CGameObject* obj = m_skinnedBatch.objectRefs[objectIndex];
		if ( !obj ) continue;

		const int meshCount = obj->GetMeshCount();
		for ( int meshIndex = 0; meshIndex < meshCount; ++meshIndex )
		{
			std::shared_ptr<CMesh> mesh = obj->GetMeshShared(meshIndex);
			if ( !mesh ) continue;

			std::string geometryKey = mesh->GetSourceMeshPath();
			if ( geometryKey.empty() )
			{
				char buf[64];
				sprintf_s(buf, "meshptr_%p", mesh.get());
				geometryKey = buf;
			}

			for ( UINT subMeshIndex = 0; subMeshIndex < ( UINT ) mesh->m_SubMeshes.size(); ++subMeshIndex )
			{
				SkinnedInstanceGroup* targetGroup = nullptr;

				for ( SkinnedInstanceGroup& group : m_skinnedInstanceGroups )
				{
					if ( group.geometryKey == geometryKey &&
						group.meshIndex == ( UINT ) meshIndex &&
						group.subMeshIndex == subMeshIndex )
					{
						targetGroup = &group;
						break;
					}
				}

				if ( !targetGroup )
				{
					SkinnedInstanceGroup newGroup{};
					newGroup.geometryKey = geometryKey;
					newGroup.mesh = mesh; // representative mesh
					newGroup.subMeshIndex = subMeshIndex;
					newGroup.meshIndex = ( UINT ) meshIndex;
					m_skinnedInstanceGroups.push_back(std::move(newGroup));
					targetGroup = &m_skinnedInstanceGroups.back();
				}

				targetGroup->objectIndices.push_back(objectIndex);
			}
		}
	}

	UINT runningStart = 0;
	for ( SkinnedInstanceGroup& group : m_skinnedInstanceGroups )
	{
		group.instanceBufferStart = runningStart;
		runningStart += ( UINT ) group.objectIndices.size();
	}

	m_skinnedInstanceBufferCapacity = runningStart;
}

void CGameScene::RenderStaticInstanceGroups(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !m_pd3dStaticInstanceBuffer ) return;
	if ( !m_pMappedStaticInstanceBuffer ) return;

	bool lastUseTreeShader = false;
	bool hasBoundAnyShader = false;

	for ( const StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		if ( !group.mesh ) continue;
		if ( group.subMeshIndex >= group.mesh->m_SubMeshes.size() ) continue;

		const SubMesh& sm = group.mesh->m_SubMeshes[group.subMeshIndex];
		if ( sm.indices.empty() ) continue;

		const UINT maxInstanceCount = ( UINT ) group.objectIndices.size();
		if ( maxInstanceCount == 0 ) continue;

		const UINT instanceBase = group.instanceBufferStart;
		if ( ( instanceBase + maxInstanceCount ) > m_staticInstanceBufferCapacity ) continue;

		UINT visibleInstanceCount = 0;

		for ( UINT i = 0; i < maxInstanceCount; ++i )
		{
			const UINT objectIndex = group.objectIndices[i];
			if ( objectIndex >= ( UINT ) m_staticBatch.objectRefs.size() ) continue;

			if ( objectIndex < ( UINT ) m_staticDistanceCullFlags.size() )
			{
				if ( m_staticDistanceCullFlags[objectIndex] != 0 )
					continue;
			}

			CGameObject* obj = m_staticBatch.objectRefs[objectIndex];
			if ( !obj ) continue;
			if ( !obj->IsVisible(camera) ) continue;

			auto* renderer = obj->GetComponent<CStaticMeshRendererComponent>();
			if ( !renderer ) continue;
			if ( !renderer->IsEnabled() ) continue;

			StaticInstanceVertex& dst = m_pMappedStaticInstanceBuffer[instanceBase + visibleInstanceCount];
			ZeroMemory(&dst, sizeof(dst));

			const XMFLOAT4X4& W = obj->GetWorldMatrix();

			dst.world0 = XMFLOAT4(W._11, W._12, W._13, W._14);
			dst.world1 = XMFLOAT4(W._21, W._22, W._23, W._24);
			dst.world2 = XMFLOAT4(W._31, W._32, W._33, W._34);
			dst.world3 = XMFLOAT4(W._41, W._42, W._43, W._44);
			dst.objectId = objectIndex;

			++visibleInstanceCount;
		}

		if ( visibleInstanceCount == 0 ) continue;

		D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
		vbViews[0] = sm.vbView;
		vbViews[1].BufferLocation =
			m_pd3dStaticInstanceBuffer->GetGPUVirtualAddress() +
			( UINT64 ) ( sizeof(StaticInstanceVertex) * instanceBase );
		vbViews[1].SizeInBytes = sizeof(StaticInstanceVertex) * visibleInstanceCount;
		vbViews[1].StrideInBytes = sizeof(StaticInstanceVertex);


		if ( !hasBoundAnyShader || ( lastUseTreeShader != group.useTreeShader ) )
		{
			if ( group.useTreeShader )
			{
				if ( m_treeStaticShader )
					m_treeStaticShader->Render(cmd, camera, &m_staticBatch);
				else if ( m_staticBatch.shader )
					m_staticBatch.shader->Render(cmd, camera, &m_staticBatch);
			}
			else
			{
				if ( m_staticBatch.shader )
					m_staticBatch.shader->Render(cmd, camera, &m_staticBatch);
			}

			lastUseTreeShader = group.useTreeShader;
			hasBoundAnyShader = true;
		}

		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		const UINT mid = ( sm.materialId == 0xFFFFFFFFu ) ? 0u : sm.materialId;
		cmd->SetGraphicsRoot32BitConstant(ROOT_PARAMETER_MATERIAL_ID, mid, 0);

		if ( sm.material && sm.material->NeedsLegacyBinding() )
			sm.material->UpdateShaderVariables(cmd);

		cmd->IASetVertexBuffers(0, 2, vbViews);
		cmd->IASetIndexBuffer(&sm.ibView);

		cmd->DrawIndexedInstanced(( UINT ) sm.indices.size(), visibleInstanceCount, 0, 0, 0);
	}
}

void CGameScene::RenderSkinnedInstanceGroups(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !m_pd3dSkinnedInstanceBuffer ) return;
	if ( !m_pMappedSkinnedInstanceBuffer ) return;
	if ( !m_pd3dSkinnedBonePaletteBuffer ) return;
	if ( !m_pMappedSkinnedBonePaletteBuffer ) return;

	cmd->SetGraphicsRootShaderResourceView(
		ROOT_PARAMETER_BONE_PALETTE,
		m_pd3dSkinnedBonePaletteBuffer->GetGPUVirtualAddress()
	);

	for ( const SkinnedInstanceGroup& group : m_skinnedInstanceGroups )
	{
		if ( !group.mesh ) continue;
		if ( group.subMeshIndex >= group.mesh->m_SubMeshes.size() ) continue;

		const SubMesh& repSm = group.mesh->m_SubMeshes[group.subMeshIndex];
		if ( repSm.indices.empty() ) continue;

		const UINT maxInstanceCount = ( UINT ) group.objectIndices.size();
		if ( maxInstanceCount == 0 ) continue;

		const UINT instanceBase = group.instanceBufferStart;
		if ( ( instanceBase + maxInstanceCount ) > m_skinnedInstanceBufferCapacity ) continue;

		UINT visibleInstanceCount = 0;

		for ( UINT i = 0; i < maxInstanceCount; ++i )
		{
			const UINT objectIndex = group.objectIndices[i];
			if ( objectIndex >= ( UINT ) m_skinnedBatch.objectRefs.size() ) continue;

			if ( objectIndex < ( UINT ) m_skinnedDistanceCullFlags.size() )
			{
				if ( m_skinnedDistanceCullFlags[objectIndex] != 0 )
					continue;
			}

			CGameObject* obj = m_skinnedBatch.objectRefs[objectIndex];
			if ( !obj ) continue;
			if ( !obj->IsVisible(camera) ) continue;

			auto* renderer = obj->GetComponent<CSkinnedMeshRendererComponent>();
			if ( !renderer ) continue;
			if ( !renderer->IsEnabled() ) continue;

			auto* skin = obj->GetComponent<CSkinningComponent>();
			if ( !skin ) continue;
			if ( !skin->IsSkinned() ) continue;

			std::shared_ptr<CMesh> objMesh = obj->GetMeshShared(( int ) group.meshIndex); 
			if ( !objMesh ) continue;
			if ( group.subMeshIndex >= objMesh->m_SubMeshes.size() ) continue;

			const SubMesh& objSm = objMesh->m_SubMeshes[group.subMeshIndex];

			SkinnedInstanceVertex& dst =
				m_pMappedSkinnedInstanceBuffer[instanceBase + visibleInstanceCount];
			ZeroMemory(&dst, sizeof(dst));

			const XMFLOAT4X4& W = obj->GetWorldMatrix();

			dst.world0 = XMFLOAT4(W._11, W._12, W._13, W._14);
			dst.world1 = XMFLOAT4(W._21, W._22, W._23, W._24);
			dst.world2 = XMFLOAT4(W._31, W._32, W._33, W._34);
			dst.world3 = XMFLOAT4(W._41, W._42, W._43, W._44);

			dst.materialId = ( objSm.materialId == 0xFFFFFFFFu ) ? 0u : objSm.materialId;
			dst.bonePaletteBase = objectIndex * m_skinnedBonePaletteStride;

			const XMFLOAT4X4* srcBoneMats = skin->GetMappedBoneMatrices();
			const UINT boneCount = ( UINT ) skin->GetBoneCount();

			if ( srcBoneMats && boneCount > 0 )
			{
				memcpy(
					m_pMappedSkinnedBonePaletteBuffer + dst.bonePaletteBase,
					srcBoneMats,
					sizeof(XMFLOAT4X4) * boneCount
				);
			}

			++visibleInstanceCount;
		}

		if ( visibleInstanceCount == 0 ) continue;

		D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
		vbViews[0] = repSm.vbView;
		vbViews[1].BufferLocation =
			m_pd3dSkinnedInstanceBuffer->GetGPUVirtualAddress() +
			( UINT64 ) ( sizeof(SkinnedInstanceVertex) * instanceBase );
		vbViews[1].SizeInBytes = sizeof(SkinnedInstanceVertex) * visibleInstanceCount;
		vbViews[1].StrideInBytes = sizeof(SkinnedInstanceVertex);

		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 2, vbViews);
		cmd->IASetIndexBuffer(&repSm.ibView);

		cmd->DrawIndexedInstanced(
			( UINT ) repSm.indices.size(),
			visibleInstanceCount,
			0,
			0,
			0
		);
	}
}

void CGameScene::BuildSkinnedBatch(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	const std::shared_ptr<CSkinnedObjectsShader>& pSkinnedShader,
	UINT nRenderTargets,
	DXGI_FORMAT* rtvFormats,
	DXGI_FORMAT dsvFormat
)
{
	auto* b = &m_skinnedBatch;
	if ( !b ) return;

	const UINT cap = b->capacity;
	if ( cap == 0 ) return;

	pSkinnedShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		nRenderTargets,
		rtvFormats,
		dsvFormat
	);

	b->cbElementBytes = ( ( sizeof(CB_GAMEOBJECT_INFO) + 255 ) & ~255 );

	b->cbGameObjects = ::CreateBufferResource(
		dev, cmd, nullptr,
		b->cbElementBytes * cap,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	b->cbGameObjects->Map(0, nullptr, ( void** ) &b->mappedGameObjects);

	b->baseCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
	b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	m_pDescriptorHeap->CreateConstantBufferViews(
		dev,
		cap,
		b->cbGameObjects.Get(),
		b->cbElementBytes
	);

	m_skinnedObjects.clear();
	m_skinnedObjects.reserve(cap);

	b->objectRefs.clear();
	b->objectRefs.reserve(cap);

	b->count = 0;
	ResetSkinnedWorldLodEntries();

	m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };

	auto MakeSkinnedContext = [ & ] (UINT objectIndex)
		{
			GameSceneObjectFactory::CreateContext ctx{};
			ctx.device = dev;
			ctx.cmd = cmd;
			ctx.mappedGameObjectCB =
				reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >( b->mappedGameObjects ) +
					objectIndex * b->cbElementBytes
				);
			ctx.cbvGpuHandle.ptr =
				b->baseCbvGpu.ptr + ( UINT64 ) objectIndex * b->cbvInc;
			return ctx;
		};

	auto ApplyPlayerBodyCollider =
		[ ] (GameSceneObjectFactory::SkinnedRenderableDesc& desc)
		{
			desc.addCollider = true;
			desc.colliderType = EColliderType::BCapsule;
			desc.colliderLayer = kCollisionLayerPlayer;
			desc.colliderMask =
				CollisionBit(kCollisionLayerWorldStatic) |
				CollisionBit(kCollisionLayerMonsterWeapon);
			desc.colliderEnabled = true;
		};

	auto ApplyMonsterBodyCollider =
		[ ] (GameSceneObjectFactory::SkinnedRenderableDesc& desc)
		{
			desc.addCollider = true;
			desc.colliderType = EColliderType::BCapsule;
			desc.colliderLayer = kCollisionLayerMonster;
			desc.colliderMask = CollisionBit(kCollisionLayerPlayerWeapon);
			desc.colliderEnabled = true;
		};

	auto RegisterSkinnedCullEntry =
		[this](
			CGameObject* raw,
			UINT objectIndex,
			const char* assetName,
			const XMFLOAT3& pos,
			const std::array<std::shared_ptr<CMesh>, 3>& lodMeshes,
			bool lodEnabled,
			float lodDistance01,
			float lodDistance12,
			float cullDistance)
		{
			if ( !raw || !assetName || !assetName[0] )
				return;

			SkinnedWorldLodEntry entry{};
			entry.object = raw;
			entry.skinnedBatchObjectIndex = objectIndex;
			entry.assetName = assetName;
			entry.lodReferencePosition = pos;

			entry.lodEnabled = lodEnabled;
			entry.currentLod = 0;
			entry.lodDistance01 = lodDistance01;
			entry.lodDistance12 = lodDistance12;
			entry.lodMeshes = lodMeshes;

			entry.distanceCullEnabled = true;
			entry.distanceCulled = false;
			entry.cullDistance = cullDistance;

			if ( !entry.lodMeshes[0] )
				entry.lodMeshes[0] = raw->GetMeshShared(0);

			m_skinnedWorldLodEntries.push_back(std::move(entry));
		};

	const UINT fighterCount = m_PlayerCount;
	const XMFLOAT3 playerBase(0.0f, 0.0f, -150.0f);

	m_swordManRefs.clear();
	m_swordManRefs.reserve(m_swordManCount);

	m_bowManRefs.clear();
	m_bowManRefs.reserve(m_bowManCount);

	m_MutantRefs.clear();
	m_MutantRefs.reserve(m_MutantCount);

#ifdef USING_NETWORK
	GameStartData gameStartData{};
	if ( std::holds_alternative<GameStartData>(m_pendingNetworkMessage.data) )
	{
		gameStartData = std::get<GameStartData>(m_pendingNetworkMessage.data);
	}

	auto GetNetworkEnemySpawn = [ & ] (UINT index, XMFLOAT3& outPos, float& outYaw) -> bool
		{
			if ( index >= static_cast< UINT >( gameStartData.enemies.size() ) )
				return false;

			const auto& state = gameStartData.enemies[index];
			outPos = state.position;
			outYaw = state.yaw;
			return true;
		};

	auto GetNetworkPlayerSpawn = [ & ] (UINT index, XMFLOAT3& outPos, float& outYaw, EWeaponType& outWeapon) -> bool
		{
			if ( index >= static_cast< UINT >( gameStartData.players.size() ) )
				return false;

			const auto& state = gameStartData.players[index];
			outPos = state.position;
			outYaw = state.yaw;
			outWeapon = state.weaponType;
			return true;
		};
#endif

	UINT enemyIndex = 0;

#ifndef USING_NETWORK
	auto GatherLocalMonsterSpawns = [ this ] (const char* typeName)
		{
			std::vector<const MonsterSpawnEntry*> result;
			result.reserve(m_monsterSpawnEntries.size());

			for ( const MonsterSpawnEntry& entry : m_monsterSpawnEntries )
			{
				if ( entry.type == typeName )
					result.push_back(&entry);
			}

			std::sort(
				result.begin(),
				result.end(),
				[ ] (const MonsterSpawnEntry* a, const MonsterSpawnEntry* b)
				{
					return a->index < b->index;
				}
			);

			return result;
		};

	const auto ghoulSpawns = GatherLocalMonsterSpawns("Ghoul");
	const auto swordSpawns = GatherLocalMonsterSpawns("SwordMan");
	const auto bowSpawns = GatherLocalMonsterSpawns("BowMan");
	const auto mutantSpawns = GatherLocalMonsterSpawns("Mutant");
	const auto bossSpawns = GatherLocalMonsterSpawns("Boss");
#endif

	// ------------------------------------------------------------------------
	// Ghoul
	// ------------------------------------------------------------------------
	{
#ifdef USING_NETWORK
		const UINT countW = m_ghoulCount;
#else
		const UINT countW = static_cast< UINT >( ghoulSpawns.size() );
#endif

		const auto& ghoulClips = GetGhoulClipEntries();

		std::array<std::shared_ptr<CMesh>, 3> ghoulLodMeshes = { nullptr, nullptr, nullptr };

		for ( int lodLevel = 0; lodLevel < 3; ++lodLevel )
		{
			AssetBuildDesc ghoulLodDesc{};
			if ( !ResolveGhoulSkinnedLodAssetDesc(lodLevel, ghoulLodDesc) )
				continue;

			BuiltAsset ghoulLodAsset = AssetManager::BuildAsset(
				dev, cmd,
				m_pMaterials.get(),
				ghoulLodDesc
			);

			ghoulLodMeshes[( size_t ) lodLevel] = ghoulLodAsset.mesh;
		}

		std::shared_ptr<CMesh> ghoulBaseMesh = ghoulLodMeshes[0];
		if ( !ghoulBaseMesh )
		{
			AssetBuildDesc ghoulDesc{};
			GetGameSceneAssetBuildDesc(EGameSceneAssetId::Ghoul, ghoulDesc);

			BuiltAsset ghoulAsset = AssetManager::BuildAsset(
				dev, cmd,
				m_pMaterials.get(),
				ghoulDesc
			);

			ghoulBaseMesh = ghoulAsset.mesh;
			ghoulLodMeshes[0] = ghoulBaseMesh;
		}

		GameSceneObjectFactory::PreloadClipSet(
			ghoulBaseMesh.get(),
			"Ghoul",
			ghoulClips
		);

		for ( UINT k = 0; k < countW; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			XMFLOAT3 pos{};
			float yaw = 180.0f;

#ifdef USING_NETWORK
			if ( !GetNetworkEnemySpawn(enemyIndex, pos, yaw) )
				break;
#else
			if ( k >= ghoulSpawns.size() )
				break;

			pos = ghoulSpawns[k]->pos;
			yaw = ghoulSpawns[k]->yawDeg;
#endif

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = ghoulBaseMesh;
			createDesc.position = pos;
			createDesc.yawDeg = yaw;

			ApplyMonsterBodyCollider(createDesc);

			createDesc.addAnimator = true;
			createDesc.addActorTag = true;
			createDesc.actorKind = EActorKind::NPC;
			createDesc.playerControl = EPlayerControl::None;
			createDesc.playerSlot = -1;

			createDesc.addMonsterCombat = true;
			createDesc.addMonsterWeaponHitbox = true;

			createDesc.skeletonKey = "Ghoul";
			createDesc.clipEntries = &ghoulClips;

			createDesc.initMonsterController = true;
			createDesc.monsterInitialState = EMonsterAnimState::Idle;
			createDesc.monsterProfile.idleClip = "Idle";
			createDesc.monsterProfile.moveClip = "Walk";
			createDesc.monsterProfile.runClip = "Run";
			createDesc.monsterProfile.hitClip = "Hit";
			createDesc.monsterProfile.attackClip = "Attack";
			createDesc.monsterProfile.deathClip = "Death";

			createDesc.useOwnerBoneWeaponCapsules = true;
			createDesc.monsterWeaponConfigs.push_back(
				{ "Attack", 0.20f, 0.55f, { "hand_r" } }
			);

#ifndef USING_NETWORK
			// CGhoulAIComponent는 기존 요청대로 현재 전부 비활성화 상태로 유지.
			// auto* ghoulAI = obj->AddComponent<CGhoulAIComponent>();
#endif

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

			++enemyIndex;

			CGameObject* raw = obj.get();

			RegisterSkinnedCullEntry(
				raw, i, "Ghoul", pos,
				ghoulLodMeshes, true,
				15.0f, 30.0f, 60.0f
			);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();
		}
	}

	// ------------------------------------------------------------------------
	// SwordMan
	// ------------------------------------------------------------------------
	{
#ifdef USING_NETWORK
		const UINT countX = m_swordManCount;
#else
		const UINT countX = static_cast< UINT >( swordSpawns.size() );
#endif

		const auto& swordClips = GetEnemySwordClipEntries();

		AssetBuildDesc swordManDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::SwordMan, swordManDesc);

		BuiltAsset swordManAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			swordManDesc
		);

		GameSceneObjectFactory::PreloadClipSet(
			swordManAsset.mesh.get(),
			"EnemySword",
			swordClips
		);

		for ( UINT k = 0; k < countX; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			XMFLOAT3 pos{};
			float yaw = 180.0f;

#ifdef USING_NETWORK
			if ( !GetNetworkEnemySpawn(enemyIndex, pos, yaw) )
				break;
#else
			if ( k >= swordSpawns.size() )
				break;

			pos = swordSpawns[k]->pos;
			yaw = swordSpawns[k]->yawDeg;
#endif

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = swordManAsset.mesh;
			createDesc.position = pos;
			createDesc.yawDeg = yaw;

			ApplyMonsterBodyCollider(createDesc);

			createDesc.addAnimator = true;
			createDesc.addActorTag = true;
			createDesc.actorKind = EActorKind::NPC;
			createDesc.playerControl = EPlayerControl::None;
			createDesc.playerSlot = -1;

			createDesc.addMonsterCombat = true;

			createDesc.skeletonKey = "EnemySword";
			createDesc.clipEntries = &swordClips;

			createDesc.initMonsterController = true;
			createDesc.monsterInitialState = EMonsterAnimState::Idle;
			createDesc.monsterProfile.idleClip = "Idle";
			createDesc.monsterProfile.moveClip = "Walk";
			createDesc.monsterProfile.hitClip = "Hit";
			createDesc.monsterProfile.attackClip = "Attack";
			createDesc.monsterProfile.deathClip = "Death";

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

			++enemyIndex;

			CGameObject* raw = obj.get();

			std::array<std::shared_ptr<CMesh>, 3> noLodMeshes =
			{
				swordManAsset.mesh, nullptr, nullptr
			};

			RegisterSkinnedCullEntry(
				raw, i, "SwordMan", pos,
				noLodMeshes, false,
				0.0f, 0.0f, 90.0f
			);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_swordManRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// BowMan
	// ------------------------------------------------------------------------
	{
#ifdef USING_NETWORK
		const UINT countY = m_bowManCount;
#else
		const UINT countY = static_cast< UINT >( bowSpawns.size() );
#endif

		const auto& bowManClips = GetEnemyBowClipEntries();

		AssetBuildDesc bowManDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::BowMan, bowManDesc);

		BuiltAsset bowManAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			bowManDesc
		);

		GameSceneObjectFactory::PreloadClipSet(
			bowManAsset.mesh.get(),
			"EnemyBow",
			bowManClips
		);

		for ( UINT k = 0; k < countY; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			XMFLOAT3 pos{};
			float yaw = 180.0f;

#ifdef USING_NETWORK
			if ( !GetNetworkEnemySpawn(enemyIndex, pos, yaw) )
				break;
#else
			if ( k >= bowSpawns.size() )
				break;

			pos = bowSpawns[k]->pos;
			yaw = bowSpawns[k]->yawDeg;
#endif

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = bowManAsset.mesh;
			createDesc.position = pos;
			createDesc.yawDeg = yaw;

			ApplyMonsterBodyCollider(createDesc);

			createDesc.addAnimator = true;
			createDesc.addActorTag = true;
			createDesc.actorKind = EActorKind::NPC;
			createDesc.playerControl = EPlayerControl::None;
			createDesc.playerSlot = -1;

			createDesc.addMonsterCombat = true;

			createDesc.skeletonKey = "EnemyBow";
			createDesc.clipEntries = &bowManClips;

			createDesc.initMonsterController = true;
			createDesc.monsterInitialState = EMonsterAnimState::Idle;
			createDesc.monsterProfile.idleClip = "Idle";
			createDesc.monsterProfile.moveClip = "Walk";
			createDesc.monsterProfile.hitClip = "Hit";
			createDesc.monsterProfile.deathClip = "Death";
			createDesc.monsterProfile.attackClip = "Bow_Load";
			createDesc.monsterProfile.attackNextClip = "Bow_Release";
			createDesc.monsterProfile.attackHasChain = true;

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

			++enemyIndex;

			CGameObject* raw = obj.get();

			std::array<std::shared_ptr<CMesh>, 3> noLodMeshes =
			{
				bowManAsset.mesh, nullptr, nullptr
			};

			RegisterSkinnedCullEntry(
				raw, i, "BowMan", pos,
				noLodMeshes, false,
				0.0f, 0.0f, 100.0f
			);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_bowManRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// Mutant
	// ------------------------------------------------------------------------
	{
#ifdef USING_NETWORK
		const UINT countZ = m_MutantCount;
#else
		const UINT countZ = static_cast< UINT >( mutantSpawns.size() );
#endif

		const auto& mutantClips = GetMutantClipEntries();

		AssetBuildDesc mutantDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::Mutant, mutantDesc);

		BuiltAsset mutantAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			mutantDesc
		);

		GameSceneObjectFactory::PreloadClipSet(
			mutantAsset.mesh.get(),
			"Mutant",
			mutantClips
		);

		for ( UINT k = 0; k < countZ; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			XMFLOAT3 pos{};
			float yaw = 180.0f;

#ifdef USING_NETWORK
			if ( !GetNetworkEnemySpawn(enemyIndex, pos, yaw) )
				break;
#else
			if ( k >= mutantSpawns.size() )
				break;

			pos = mutantSpawns[k]->pos;
			yaw = mutantSpawns[k]->yawDeg;
#endif

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = mutantAsset.mesh;
			createDesc.position = pos;
			createDesc.yawDeg = yaw;

			ApplyMonsterBodyCollider(createDesc);

			createDesc.addAnimator = true;
			createDesc.addActorTag = true;
			createDesc.actorKind = EActorKind::NPC;
			createDesc.playerControl = EPlayerControl::None;
			createDesc.playerSlot = -1;

			createDesc.addMonsterCombat = true;
			createDesc.addMonsterWeaponHitbox = true;

			createDesc.skeletonKey = "Mutant";
			createDesc.clipEntries = &mutantClips;

			createDesc.initMonsterController = true;
			createDesc.monsterInitialState = EMonsterAnimState::Idle;
			createDesc.monsterProfile.idleClip = "Idle";
			createDesc.monsterProfile.moveClip = "Walk";
			createDesc.monsterProfile.hitClip = "Hit";
			createDesc.monsterProfile.attackClip = "Attack";
			createDesc.monsterProfile.deathClip = "Death";

			createDesc.useOwnerBoneWeaponCapsules = true;
			createDesc.monsterWeaponConfigs.push_back(
				{ "Attack", 0.20f, 0.55f, { "CATRigRArmPalm" } }
			);

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

			++enemyIndex;

			CGameObject* raw = obj.get();

			std::array<std::shared_ptr<CMesh>, 3> noLodMeshes =
			{
				mutantAsset.mesh, nullptr, nullptr
			};

			RegisterSkinnedCullEntry(
				raw, i, "Mutant", pos,
				noLodMeshes, false,
				0.0f, 0.0f, 110.0f
			);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_MutantRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// Boss
	// ------------------------------------------------------------------------
	{
#ifdef USING_NETWORK
		const UINT countOne = m_bossCount;
#else
		const UINT countOne = static_cast< UINT >( bossSpawns.size() );
#endif

		const auto& bossClips = GetBossClipEntries();

		AssetBuildDesc bossDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::Boss, bossDesc);

		BuiltAsset bossAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			bossDesc
		);

		GameSceneObjectFactory::PreloadClipSet(
			bossAsset.mesh.get(),
			"Boss",
			bossClips
		);

		for ( UINT k = 0; k < countOne; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			XMFLOAT3 pos{};
			float yaw = 180.0f;

#ifdef USING_NETWORK
			if ( !GetNetworkEnemySpawn(enemyIndex, pos, yaw) )
				break;
#else
			if ( k >= bossSpawns.size() )
				break;

			pos = bossSpawns[k]->pos;
			yaw = bossSpawns[k]->yawDeg;
#endif

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = bossAsset.mesh;
			createDesc.position = pos;
			createDesc.yawDeg = yaw;

			ApplyMonsterBodyCollider(createDesc);

			createDesc.addAnimator = true;
			createDesc.addActorTag = true;
			createDesc.actorKind = EActorKind::NPC;
			createDesc.playerControl = EPlayerControl::None;
			createDesc.playerSlot = -1;

			createDesc.addMonsterCombat = true;
			createDesc.addMonsterWeaponHitbox = true;

			createDesc.skeletonKey = "Boss";
			createDesc.clipEntries = &bossClips;

			createDesc.initMonsterController = true;
			createDesc.monsterInitialState = EMonsterAnimState::Idle;
			createDesc.monsterProfile.idleClip = "Idle";
			createDesc.monsterProfile.moveClip = "Walk";
			createDesc.monsterProfile.hitClip = "Hit";
			createDesc.monsterProfile.deathClip = "Death";
			createDesc.monsterProfile.attackClip = "AttackLeft";
			createDesc.monsterProfile.appearClip = "Appear";
			createDesc.monsterProfile.callClip = "Call";
			createDesc.monsterProfile.spellClip = "Spell";

			createDesc.useOwnerBoneWeaponCapsules = true;
			createDesc.monsterWeaponConfigs.push_back(
				{ "AttackLeft", 0.20f, 0.55f, { "Wrist_L" } }
			);
			createDesc.monsterWeaponConfigs.push_back(
				{ "AttackRight", 0.20f, 0.55f, { "Wrist_R" } }
			);

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

			++enemyIndex;

			CGameObject* raw = obj.get();

			std::array<std::shared_ptr<CMesh>, 3> noLodMeshes =
			{
				bossAsset.mesh, nullptr, nullptr
			};

			RegisterSkinnedCullEntry(
				raw, i, "Boss", pos,
				noLodMeshes, false,
				0.0f, 0.0f, 160.0f
			);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();
		}
	}

	// ------------------------------------------------------------------------
	// Player
	// ------------------------------------------------------------------------
	{
		const auto& playerClips = GetPlayerClipEntries();

		std::array<AssetBuildDesc, 4> playerDescs{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerMesh1, playerDescs[0]);
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerMesh2, playerDescs[1]);
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerMesh3, playerDescs[2]);
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerMesh4, playerDescs[3]);

		for ( UINT k = 0; k < fighterCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();
			const int slot = ( int ) k;
			const bool isLocal = ( slot == m_localPlayerSlot );

			BuiltAsset playerAsset = AssetManager::BuildAsset(
				dev, cmd,
				m_pMaterials.get(),
				playerDescs[( size_t ) slot]
			);

			XMFLOAT3 pos{};
			float yaw = 0.0f;

#ifdef USING_NETWORK
			EWeaponType initialWeapon = EWeaponType::Sword;
			if ( !GetNetworkPlayerSpawn(k, pos, yaw, initialWeapon) )
				break;
#else
			pos.x = playerBase.x + 2.0f * ( float ) slot;
			pos.y = playerBase.y;
			pos.z = playerBase.z;
#endif

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = playerAsset.mesh;
			createDesc.position = pos;
			createDesc.yawDeg = yaw;

			ApplyPlayerBodyCollider(createDesc);

			createDesc.addAnimator = true;
			createDesc.addActorTag = true;
			createDesc.actorKind = EActorKind::Player;
			createDesc.playerControl = isLocal ? EPlayerControl::Local : EPlayerControl::Remote;
			createDesc.playerSlot = slot;

			createDesc.addPlayerController = isLocal;
			createDesc.addPlayerEquipment = true;
			createDesc.addPlayerWeaponHitbox = true;

			createDesc.skeletonKey = "Player";
			createDesc.clipEntries = &playerClips;

			createDesc.initPlayerController = true;
			createDesc.playerIdleClip = "Idle_Normal";
			createDesc.playerMoveClip = "Walk_F";
			createDesc.playerHitClip = "Hit_Normal";
			createDesc.playerAttackClip = "Attack_Sword";

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

#ifdef USING_NETWORK
			if ( auto* equipComp = obj->GetComponent<CPlayerEquipmentComponent>() )
			{
				equipComp->SetLoadout(initialWeapon);
			}
#endif

			CGameObject* raw = obj.get();

			if ( slot >= 0 && slot <= 3 )
				m_playersBySlot[( size_t ) slot] = raw;

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();
		}
	}

	// ------------------------------------------------------------------------
	// PlayerBow pool
	// ------------------------------------------------------------------------
	{
		const auto& playerBowClips = GetPlayerBowClipEntries();

		AssetBuildDesc playerBowDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerBow, playerBowDesc);

		BuiltAsset bowAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			playerBowDesc
		);

		GameSceneObjectFactory::PreloadClipSet(
			bowAsset.mesh.get(),
			"BowP",
			playerBowClips
		);

		m_PlayerBowRefs.clear();
		m_PlayerBowRefs.reserve(m_PlayerBowCount);

		for ( UINT k = 0; k < m_PlayerBowCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = bowAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addAnimator = true;
			createDesc.skeletonKey = "BowP";
			createDesc.clipEntries = &playerBowClips;

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_PlayerBowRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// EnemyBow pool
	// ------------------------------------------------------------------------
	{
		const auto& enemyBowWeaponClips = GetEnemyBowWeaponClipEntries();

		AssetBuildDesc enemyBowDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::EnemyBow, enemyBowDesc);

		BuiltAsset bowAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			enemyBowDesc
		);

		GameSceneObjectFactory::PreloadClipSet(
			bowAsset.mesh.get(),
			"BowE",
			enemyBowWeaponClips
		);

		m_EnemyBowRefs.clear();
		m_EnemyBowRefs.reserve(m_bowManCount);

		for ( UINT k = 0; k < m_bowManCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = bowAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addAnimator = true;
			createDesc.skeletonKey = "BowE";
			createDesc.clipEntries = &enemyBowWeaponClips;

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_EnemyBowRefs.push_back(raw);
		}
	}

	m_preparedBowmanArrows.assign(m_bowManRefs.size(), nullptr);
	m_prevEnemyBowReleasePhase.assign(m_bowManRefs.size(), false);

	BuildSkinnedInstanceGroups();

	if ( m_pd3dSkinnedInstanceBuffer )
	{
		if ( m_pMappedSkinnedInstanceBuffer )
		{
			m_pd3dSkinnedInstanceBuffer->Unmap(0, NULL);
			m_pMappedSkinnedInstanceBuffer = nullptr;
		}
		m_pd3dSkinnedInstanceBuffer.Reset();
	}

	if ( m_pd3dSkinnedBonePaletteBuffer )
	{
		if ( m_pMappedSkinnedBonePaletteBuffer )
		{
			m_pd3dSkinnedBonePaletteBuffer->Unmap(0, NULL);
			m_pMappedSkinnedBonePaletteBuffer = nullptr;
		}
		m_pd3dSkinnedBonePaletteBuffer.Reset();
	}

	m_skinnedBonePaletteStride = 1;
	for ( UINT i = 0; i < ( UINT ) m_skinnedBatch.objectRefs.size(); ++i )
	{
		CGameObject* obj = m_skinnedBatch.objectRefs[i];
		if ( !obj ) continue;

		const UINT boneCount = ( UINT ) obj->GetBoneCount();
		if ( boneCount > m_skinnedBonePaletteStride )
			m_skinnedBonePaletteStride = boneCount;
	}

	m_skinnedBonePaletteCapacity =
		m_skinnedBonePaletteStride * ( UINT ) m_skinnedBatch.objectRefs.size();

	if ( m_skinnedInstanceBufferCapacity > 0 )
	{
		const UINT instanceBufferBytes =
			sizeof(SkinnedInstanceVertex) * m_skinnedInstanceBufferCapacity;

		m_pd3dSkinnedInstanceBuffer = ::CreateBufferResource(
			dev, cmd, nullptr,
			instanceBufferBytes,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr
		);

		m_pd3dSkinnedInstanceBuffer->Map(
			0, nullptr, ( void** ) &m_pMappedSkinnedInstanceBuffer);
	}

	if ( m_skinnedBonePaletteCapacity > 0 )
	{
		const UINT bonePaletteBufferBytes =
			sizeof(XMFLOAT4X4) * m_skinnedBonePaletteCapacity;

		m_pd3dSkinnedBonePaletteBuffer = ::CreateBufferResource(
			dev, cmd, nullptr,
			bonePaletteBufferBytes,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr
		);

		m_pd3dSkinnedBonePaletteBuffer->Map(
			0, nullptr, ( void** ) &m_pMappedSkinnedBonePaletteBuffer);
	}
}

void CGameScene::BuildColliderBatch(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	const std::shared_ptr<CDiffusedShader>& pshader,
	UINT nRenderTargets,
	DXGI_FORMAT* rtvFormats,
	DXGI_FORMAT dsvFormat)
{
	auto* b = &m_colliderBatch;
	if ( !b ) return;

	const UINT cap = kDebugSubmeshOOBBCapacity;
	b->capacity = cap;

	if ( cap == 0 ) 
		return;

	pshader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		nRenderTargets,
		rtvFormats,
		dsvFormat
	);

	b->cbElementBytes = ( ( sizeof(CB_GAMEOBJECT_INFO) + 255 ) & ~255 );

	b->cbGameObjects = ::CreateBufferResource(
		dev, cmd, nullptr,
		b->cbElementBytes * cap,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	b->cbGameObjects->Map(0, nullptr, ( void** ) &b->mappedGameObjects);

	b->baseCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
	b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	m_pDescriptorHeap->CreateConstantBufferViews(
		dev,
		cap,
		b->cbGameObjects.Get(),
		b->cbElementBytes
	);

	m_staticObjects.clear();
	m_staticObjects.reserve(cap);

	b->objectRefs.clear();
	b->objectRefs.reserve(cap);

	m_colliderObjects.clear();
	m_colliderObjects.reserve(cap);

	b->count = 0;
	m_ColliderCount = 0;
}

void CGameScene::LinkSceneObjects()
{
	GameSceneAttachmentBinder::LinkInput input{};
	input.playerSpotFollower = m_pPlayerSpotFollower;

	CGameObject* local = GetPlayer();
	if ( !local )
		local = GetPlayerBySlot(0);

	input.preferredPlayer = local;
	input.playersBySlot = &m_playersBySlot;
	input.playerCount = m_PlayerCount;

	input.playerSwordRefs = &m_PlayerSwordRefs;
	input.playerBowRefs = &m_PlayerBowRefs;
	input.playerAxeRefs = &m_PlayerAxeRefs;
	input.playerGunRefs = &m_PlayerGunRefs;

	input.enemySwordRefs = &m_EnemySwordRefs;
	input.enemyBowRefs = &m_EnemyBowRefs;

	input.swordManRefs = &m_swordManRefs;
	input.bowManRefs = &m_bowManRefs;
	input.mutantRefs = &m_MutantRefs;
	input.helmetRefs = &m_helmetRefs;

#ifndef USING_NETWORK
	input.applyOfflineTestLoadout = true;
#else
	input.applyOfflineTestLoadout = false;
#endif

	GameSceneAttachmentBinder::LinkSceneObjects(input, m_attachmentBinds);
}

void CGameScene::CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* target)
{
	m_pMainCameraObject = std::make_unique<CGameObject>(0);

	auto* cam = m_pMainCameraObject->AddComponent<CThirdPersonCamera>();
	m_pMainCamera = cam;

	cam->SetMode(THIRD_PERSON_CAMERA);
	cam->SetTarget(target);

	cam->SetTimeLag(0.0f);

	cam->SetOffset(XMFLOAT3(0.0f, 0.0f, -2.0f));

	cam->GetPitch() = 12.0f;

	if ( target )
	{
		const XMFLOAT4X4& W = target->GetWorldMatrix();
		cam->GetYaw() = XMConvertToDegrees(std::atan2f(W._31, W._33));
	}
	else
	{
		cam->GetYaw() = 0.0f;
	}

	cam->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
	cam->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
	cam->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);

	m_pMainCameraObject->CreateComponents(dev, cmd);

	if ( target )
	{
		XMFLOAT3 lookAt = target->GetPosition();
		static constexpr float kCameraLookAtHeight = 1.7f;
		lookAt.y += kCameraLookAtHeight;

		cam->Update(lookAt, 0.0f);
		cam->SetLookAt(lookAt);
		cam->RegenerateViewMatrix();
	}
}

int CGameScene::AddUISprite(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	const char* name,
	const wchar_t* texturePath,
	const XMFLOAT4& rect,
	EUIRenderLayer layer,
	bool visible)
{
	if ( !dev || !cmd || !name || !texturePath )
		return -1;

	UISpriteEntry entry{};
	entry.name = name;
	entry.layer = layer;
	entry.visible = visible;
	entry.rect = rect;

	entry.texture = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 0);
	entry.texture->LoadTextureFromFile(
		dev,
		cmd,
		texturePath,
		RESOURCE_TEXTURE2D,
		0
	);

	CScene::m_pDescriptorHeap->CreateShaderResourceViewsOther(
		dev,
		entry.texture.get(),
		ROOT_PARAMETER_GLOBAL_SRV
	);

	entry.srvIndex = entry.texture->GetSrvIndex(0);

	// width/height를 0 이하로 넣으면 원본 텍스처 크기 사용
	if ( entry.rect.z <= 0.0f )
		entry.rect.z = static_cast< float >( entry.texture->GetTextureWidth(0) );
	if ( entry.rect.w <= 0.0f )
		entry.rect.w = static_cast< float >( entry.texture->GetTextureHeight(0) );

	m_uiSprites.push_back(std::move(entry));
	return static_cast< int >( m_uiSprites.size() - 1 );
}

void CGameScene::BuildUIResources(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
	if ( m_uiRectShader )
		m_uiRectShader->ReleaseShaderVariables();

	m_uiRectShader.reset();
	m_uiSprites.clear();
	m_pauseUISpriteIndex = -1;

	m_uiRectShader = std::make_shared<CRectUIShader>();

	DXGI_FORMAT overlayRtv = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT overlayDsv = DXGI_FORMAT_UNKNOWN;

	m_uiRectShader->CreateShader(
		dev,
		GetGraphicsRootSignature(),
		1,
		&overlayRtv,
		overlayDsv
	);
	m_uiRectShader->CreateShaderVariables(dev, cmd);

	// --------------------------------------------------------------------
	// UI layout tuning block
	// - rect = (centerX, centerY, width, height)
	// --------------------------------------------------------------------
	constexpr int kItemSlotCount = 5;
	constexpr int kEquipSlotCount = 2;

	const float itemFrameCenterX = FRAME_BUFFER_WIDTH - 105.0f;
	const float itemFrameCenterY = FRAME_BUFFER_HEIGHT - 22.5f;
	const float itemFrameWidth = 210.0f;
	const float itemFrameHeight = 45.0f;

	const float itemSlotSize = 32.0f;
	const float itemSlotSpacing = 37.0f;
	const float itemSlotStartX =
		itemFrameCenterX - ( ( kItemSlotCount - 1 ) * itemSlotSpacing * 0.5f );
	const float itemSlotCenterY = itemFrameCenterY;

	const float equipFrameCenterX = FRAME_BUFFER_WIDTH - 45.0f;
	const float equipFrameCenterY = FRAME_BUFFER_HEIGHT - 66.0f;
	const float equipFrameWidth = 90.0f;
	const float equipFrameHeight = 45.0f;

	const float equipSlotSize = 32.0f;
	const float equipSlotSpacing = 37.0f;
	const float equipSlotStartX =
		equipFrameCenterX - ( ( kEquipSlotCount - 1 ) * equipSlotSpacing * 0.5f );
	const float equipSlotCenterY = equipFrameCenterY;

	const float hpFrameCenterX = 150.0f;
	const float hpFrameCenterY = 20.0f;
	const float hpFrameWidth = 300.0f;
	const float hpFrameHeight = 40.0f;

	const float hpBarCenterX = hpFrameCenterX;
	const float hpBarCenterY = hpFrameCenterY;
	const float hpBarWidth = 290.0f;
	const float hpBarHeight = 34.0f;

	// --------------------------------------------------------------------
	// Frame layer (가장 아래)
	// --------------------------------------------------------------------
	AddUISprite(
		dev, cmd,
		"ItemFrame",
		L"Assets/UI/low_darkness_bar.dds",
		XMFLOAT4(itemFrameCenterX, itemFrameCenterY, itemFrameWidth, itemFrameHeight),
		EUIRenderLayer::Frame,
		true
	);

	AddUISprite(
		dev, cmd,
		"EquipmentFrame",
		L"Assets/UI/low_darkness_bar.dds",
		XMFLOAT4(equipFrameCenterX, equipFrameCenterY, equipFrameWidth, equipFrameHeight),
		EUIRenderLayer::Frame,
		true
	);

	AddUISprite(
		dev, cmd,
		"HPFrame",
		L"Assets/UI/low_darkness_bar.dds",
		XMFLOAT4(hpFrameCenterX, hpFrameCenterY, hpFrameWidth, hpFrameHeight),
		EUIRenderLayer::Frame,
		true
	);

	// --------------------------------------------------------------------
	// Content layer (Frame 위)
	// --------------------------------------------------------------------
	for ( int i = 0; i < kItemSlotCount; ++i )
	{
		const float centerX = itemSlotStartX + ( itemSlotSpacing * i );

		char name[64] = {};
		sprintf_s(name, "ItemSlot_%d", i);

		AddUISprite(
			dev, cmd,
			name,
			L"Assets/UI/mini_dark_bar1.dds",
			XMFLOAT4(centerX, itemSlotCenterY, itemSlotSize, itemSlotSize),
			EUIRenderLayer::Content,
			true
		);
	}

	for ( int i = 0; i < kEquipSlotCount; ++i )
	{
		const float centerX = equipSlotStartX + ( equipSlotSpacing * i );

		char name[64] = {};
		sprintf_s(name, "EquipmentSlot_%d", i);

		AddUISprite(
			dev, cmd,
			name,
			L"Assets/UI/mini_dark_bar1.dds",
			XMFLOAT4(centerX, equipSlotCenterY, equipSlotSize, equipSlotSize),
			EUIRenderLayer::Content,
			true
		);
	}

	AddUISprite(
		dev, cmd,
		"HPFill",
		L"Assets/UI/HP.dds",
		XMFLOAT4(hpBarCenterX, hpBarCenterY, hpBarWidth, hpBarHeight),
		EUIRenderLayer::Content,
		true
	);

	// --------------------------------------------------------------------
	// Pause layer (가장 위)
	// --------------------------------------------------------------------
	m_pauseUISpriteIndex = AddUISprite(
		dev, cmd,
		"Pause",
		L"Assets/UI/Pause.dds",
		XMFLOAT4(
			FRAME_BUFFER_WIDTH * 0.5f,
			FRAME_BUFFER_HEIGHT * 0.5f,
			0.0f,
			0.0f
		),
		EUIRenderLayer::Pause,
		true
	);

	if ( m_pauseUISpriteIndex >= 0 &&
		m_pauseUISpriteIndex < static_cast< int >(m_uiSprites.size()) )
	{
		UISpriteEntry& pause = m_uiSprites[( size_t ) m_pauseUISpriteIndex];

		float drawW = static_cast< float >(pause.texture->GetTextureWidth(0));
		float drawH = static_cast< float >(pause.texture->GetTextureHeight(0));

		if ( drawW <= 0.0f || drawH <= 0.0f )
		{
			drawW = 512.0f;
			drawH = 512.0f;
		}

		float fitScale = 1.0f;
		const float scaleX = static_cast< float >( FRAME_BUFFER_WIDTH ) / drawW;
		const float scaleY = static_cast< float >( FRAME_BUFFER_HEIGHT ) / drawH;

		if ( scaleX < fitScale ) fitScale = scaleX;
		if ( scaleY < fitScale ) fitScale = scaleY;
		if ( fitScale > 1.0f ) fitScale = 1.0f;

		pause.rect.z = drawW * fitScale;
		pause.rect.w = drawH * fitScale;
	}
}

void CGameScene::RenderUI(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !m_uiRectShader ) return;
	if ( m_uiSprites.empty() ) return;

	m_uiRectShader->ResetDrawOptionWriteIndex();

	for ( int layerValue = static_cast< int >( EUIRenderLayer::Frame );
		layerValue <= static_cast< int >( EUIRenderLayer::Pause );
		++layerValue )
	{
		for ( size_t i = 0; i < m_uiSprites.size(); ++i )
		{
			const UISpriteEntry& ui = m_uiSprites[i];

			if ( static_cast< int >(ui.layer) != layerValue )
				continue;

			if ( !ui.texture || ui.srvIndex == UINT_MAX )
				continue;

			bool visible = ui.visible;

			// Pause는 기존 플래그로 제어
			if ( static_cast< int >( i ) == m_pauseUISpriteIndex )
				visible = visible && m_bInactiveOverlayVisible;

			if ( !visible )
				continue;

			PS_CB_DRAW_OPTIONS opt{};
			opt.m_xmn4DrawOptions = XMINT4('T', 0, 0, 0);
			opt.m_xmu4PostSrvIdx0 = XMUINT4(ui.srvIndex, 0, 0, 0);
			opt.m_xmu4PostSrvIdx1 = XMUINT4(0, 0, 0, 0);
			opt.m_xmf4UiRect = ui.rect;
			opt.m_xmf4Viewport = XMFLOAT4(
				static_cast< float >( FRAME_BUFFER_WIDTH ),
				static_cast< float >( FRAME_BUFFER_HEIGHT ),
				1.0f / static_cast< float >( FRAME_BUFFER_WIDTH ),
				1.0f / static_cast< float >( FRAME_BUFFER_HEIGHT )
			);

			m_uiRectShader->Render(cmd, camera, &opt);
		}
	}
}

bool CGameScene::GetPauseOverlayRect(XMFLOAT4& outRect) const
{
	if ( m_pauseUISpriteIndex < 0 )
		return false;

	if ( m_pauseUISpriteIndex >= static_cast< int >(m_uiSprites.size()) )
		return false;

	const UISpriteEntry& pause = m_uiSprites[( size_t ) m_pauseUISpriteIndex];
	if ( !pause.texture || pause.srvIndex == UINT_MAX )
		return false;

	outRect = pause.rect;
	return true;
}

bool CGameScene::IsPointInPauseOverlay(POINT clientPt) const
{
    XMFLOAT4 rect{};
    if (!GetPauseOverlayRect(rect))
        return false;

    const float left = rect.x - rect.z * 0.5f;
    const float right = rect.x + rect.z * 0.5f;
    const float top = rect.y - rect.w * 0.5f;
    const float bottom = rect.y + rect.w * 0.5f;

    const float px = static_cast<float>(clientPt.x);
    const float py = static_cast<float>(clientPt.y);

    return (px >= left && px <= right && py >= top && py <= bottom);
}

void CGameScene::SetMaterialDiffuseSrvIndex(int materialId, UINT srvIndex)
{
    if (!m_pMaterials) return;
    if (materialId < 0 || materialId >= MAX_MATERIALS) return;
    m_pMaterials->m_pReflections[materialId].m_xmn4TextureIndices.x = srvIndex;
}

CGameObject* CGameScene::GetDemoFighter(int index) const
{
    if (index < 0 || index >= 3) return nullptr;
    return GetPlayerBySlot(index + 1);
}

void CGameScene::RequestDemoFighterAttack(int index)
{
    if (index < 0 || index >= 3) return;
    RequestPlayerAttackBySlot(index + 1);
}

void CGameScene::RequestPlayerAttackBySlot(int slot)
{
#ifdef USING_NETWORK
	UNREFERENCED_PARAMETER(slot);
	return;
#else
	CGameObject* obj = GetPlayerBySlot(slot);
	if ( !obj ) return;

	constexpr float kArrowPullBackDistance = 0.35f;
	constexpr float kBulletSpeed = 10.0f;
	constexpr float kBulletLife = 3.0f;

	bool shouldPrepareArrow = false;
	bool shouldFireBullet = false;

	if ( auto* equip = obj->GetComponent<CPlayerEquipmentComponent>() )
	{
		const EWeaponType weapon = equip->GetEquippedWeapon();
		shouldPrepareArrow = ( weapon == EWeaponType::Bow );
		shouldFireBullet = ( weapon == EWeaponType::Gun );
	}

	if ( auto* animComp = obj->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->EnsureController() )
		{
			const bool accepted = ctrl->RequestAttack();

			if ( accepted && shouldPrepareArrow )
				RequestPrepareArrow(obj, kArrowPullBackDistance);

			if ( accepted && shouldFireBullet )
				RequestFireBullet(obj, kBulletSpeed, kBulletLife);

			return;
		}
	}

	if ( auto* ctrl = obj->GetAnimController() )
	{
		const bool accepted = ctrl->RequestAttack();

		if ( accepted && shouldPrepareArrow )
			RequestPrepareArrow(obj, kArrowPullBackDistance);

		if ( accepted && shouldFireBullet )
			RequestFireBullet(obj, kBulletSpeed, kBulletLife);

		return;
	}
#endif
}

int CGameScene::GetPlayerSlotFromObject(const CGameObject* obj) const
{
    if (!obj) return -1;

    auto* tag = obj->GetComponent<CActorTagComponent>();
    if (!tag) return -1;
    if (tag->kind != EActorKind::Player) return -1;
    if (tag->playerSlot < 0 || tag->playerSlot > 3) return -1;

    return tag->playerSlot;
}

int CGameScene::GetBowManIndexFromObject(const CGameObject* obj) const
{
	if ( !obj ) return -1;

	for ( size_t i = 0; i < m_bowManRefs.size(); ++i )
	{
		if ( m_bowManRefs[i] == obj )
			return static_cast< int >(i);
	}

	return -1;
}

void CGameScene::RequestPrepareArrow(CGameObject* shooter, float pullBackDistance)
{
    if (!shooter) return;

    auto* equip = shooter->GetComponent<CPlayerEquipmentComponent>();
    if (!equip) return;
    if (equip->GetEquippedWeapon() != EWeaponType::Bow) return;

    CGameObject* bowObj = equip->GetWeaponObject(EWeaponType::Bow);
    if (!bowObj) return;

    const int slot = GetPlayerSlotFromObject(shooter);
    if (slot < 0 || slot > 3) return;

    // 이미 준비된 화살이 있으면 중복 생성 안 함
    if (m_preparedPlayerArrows[(size_t)slot])
        return;

    for (CGameObject* arrowObj : m_arrowRefs)
    {
        if (!arrowObj) continue;

        auto* arrow = arrowObj->GetComponent<CArrowComponent>();
        if (!arrow) continue;

        if (arrow->IsActive()) continue;

		arrow->Prepare(bowObj, shooter, pullBackDistance, true, true);
		m_preparedPlayerArrows[( size_t ) slot] = arrowObj;
        return;
    }
}

void CGameScene::RequestPrepareBowmanArrow(CGameObject* bowman, float pullBackDistance)
{
	if ( !bowman ) return;

	const int bowmanIndex = GetBowManIndexFromObject(bowman);
	if ( bowmanIndex < 0 ) return;

	const size_t idx = static_cast< size_t >(bowmanIndex);

	if ( idx >= m_preparedBowmanArrows.size() ) return;
	if ( idx >= m_EnemyBowRefs.size() ) return;

	// 이미 준비된 화살이 있으면 중복 생성 안 함
	if ( m_preparedBowmanArrows[idx] )
		return;

	CGameObject* bowObj = m_EnemyBowRefs[idx];
	if ( !bowObj ) return;

	for ( CGameObject* arrowObj : m_arrowRefs )
	{
		if ( !arrowObj ) continue;

		auto* arrow = arrowObj->GetComponent<CArrowComponent>();
		if ( !arrow ) continue;
		if ( arrow->IsActive() ) continue;

		arrow->Prepare(bowObj, bowman, pullBackDistance, false, true);
		m_preparedBowmanArrows[idx] = arrowObj;
		return;
	}
}

void CGameScene::RequestReleasePreparedBowmanArrow(CGameObject* bowman, float speed, float lifeSec)
{
	if ( !bowman ) return;

	const int bowmanIndex = GetBowManIndexFromObject(bowman);
	if ( bowmanIndex < 0 ) return;

	const size_t idx = static_cast< size_t >(bowmanIndex);
	if ( idx >= m_preparedBowmanArrows.size() ) return;

	CGameObject* arrowObj = m_preparedBowmanArrows[idx];
	if ( !arrowObj ) return;

	auto* arrow = arrowObj->GetComponent<CArrowComponent>();
	if ( !arrow || !arrow->IsPrepared() )
	{
		m_preparedBowmanArrows[idx] = nullptr;
		return;
	}

	if ( !arrow->LaunchPrepared(speed, lifeSec, true) )
	{
		m_preparedBowmanArrows[idx] = nullptr;
		return;
	}

	m_preparedBowmanArrows[idx] = nullptr;
}

void CGameScene::RequestReleasePreparedArrow(CGameObject* shooter, float speed, float lifeSec)
{
    if (!shooter) return;

    const int slot = GetPlayerSlotFromObject(shooter);
    if (slot < 0 || slot > 3) return;

    CGameObject* arrowObj = m_preparedPlayerArrows[(size_t)slot];
    if (!arrowObj) return;

    auto* arrow = arrowObj->GetComponent<CArrowComponent>();
    if (!arrow || !arrow->IsPrepared())
    {
        m_preparedPlayerArrows[(size_t)slot] = nullptr;
        return;
    }

	if ( !arrow->LaunchPrepared(speed, lifeSec, true) )
	{
		m_preparedPlayerArrows[( size_t ) slot] = nullptr;
		return;
	}

	m_preparedPlayerArrows[( size_t ) slot] = nullptr;
}

void CGameScene::UpdatePreparedBowArrows()
{
	constexpr float kArrowSpeed = 3.0f;
	constexpr float kArrowLife = 6.0f;

	constexpr float kEnemyArrowPullBackDistance = 0.35f * 1.5f;
	constexpr float kEnemyArrowSpeed = 3.0f;
	constexpr float kEnemyArrowLife = 6.0f;

    for (int slot = 0; slot < 4; ++slot)
    {
        CGameObject* player = GetPlayerBySlot(slot);

        bool isBowLoad = false;
        bool isBowRelease = false;
        bool hasBowEquipped = false;

        if (player)
        {
            if (auto* equip = player->GetComponent<CPlayerEquipmentComponent>())
            {
                hasBowEquipped = (equip->GetEquippedWeapon() == EWeaponType::Bow);
            }

            if (auto* animComp = player->GetComponent<CAnimatorComponent>())
            {
                if (auto* ctrl = animComp->EnsureController())
                {
                    isBowLoad = ctrl->IsBowLoadPhase();
                    isBowRelease = ctrl->IsBowReleasePhase();
                }
            }
            else if (auto* ctrl = player->GetAnimController())
            {
                isBowLoad = ctrl->IsBowLoadPhase();
                isBowRelease = ctrl->IsBowReleasePhase();
            }
        }

        // Bow_Release 진입 순간에만 발사
        if (isBowRelease && !m_prevBowReleasePhase[(size_t)slot])
        {
            RequestReleasePreparedArrow(player, kArrowSpeed, kArrowLife);
        }

        // 공격이 끝났거나 장비가 바뀌면 준비 화살 정리
        if ((!hasBowEquipped || (!isBowLoad && !isBowRelease)) && m_preparedPlayerArrows[(size_t)slot])
        {
            if (auto* arrow = m_preparedPlayerArrows[(size_t)slot]->GetComponent<CArrowComponent>())
            {
                arrow->Deactivate();
            }
            m_preparedPlayerArrows[(size_t)slot] = nullptr;
        }

        m_prevBowReleasePhase[(size_t)slot] = isBowRelease;
    }
	for ( size_t i = 0; i < m_bowManRefs.size(); ++i )
	{
		CGameObject* bowman = m_bowManRefs[i];

		bool isBowLoad = false;
		bool isBowRelease = false;

		if ( bowman )
		{
			if ( auto* animComp = bowman->GetComponent<CAnimatorComponent>() )
			{
				if ( auto* ctrl = animComp->EnsureMonsterController() )
				{
					isBowLoad = ctrl->IsAttackPrimaryPhase();   // Bow_Load
					isBowRelease = ctrl->IsAttackChainPhase();  // Bow_Release
				}
			}
		}

		// Bow_Load 상태이면 준비 화살 생성
		if ( isBowLoad )
		{
			if ( i < m_preparedBowmanArrows.size() && m_preparedBowmanArrows[i] == nullptr )
			{
				RequestPrepareBowmanArrow(bowman, kEnemyArrowPullBackDistance);
			}
		}

		// Bow_Release 진입 순간에만 발사
		if ( i < m_prevEnemyBowReleasePhase.size() )
		{
			if ( isBowRelease && !m_prevEnemyBowReleasePhase[i] )
			{
				RequestReleasePreparedBowmanArrow(bowman, kEnemyArrowSpeed, kEnemyArrowLife);
			}
		}

		// 공격이 끝났거나 다른 액션으로 빠지면 준비 화살 정리
		if ( ( !isBowLoad && !isBowRelease ) &&
			i < m_preparedBowmanArrows.size() &&
			m_preparedBowmanArrows[i] )
		{
			if ( auto* arrow = m_preparedBowmanArrows[i]->GetComponent<CArrowComponent>() )
			{
				arrow->Deactivate();
			}
			m_preparedBowmanArrows[i] = nullptr;
		}

		if ( i < m_prevEnemyBowReleasePhase.size() )
		{
			m_prevEnemyBowReleasePhase[i] = isBowRelease;
		}
	}
}


CGameObject* CGameScene::GetPlayerBySlot(int slot) const
{
    if (slot < 0 || slot > 3) return nullptr;
    return m_playersBySlot[(size_t)slot];
}

bool CGameScene::IsLocalPlayerInsideMegaGridCenter() const
{
#ifndef USING_NETWORK
	if ( !m_spatialGridInitialized )
		return false;

	if ( m_localPlayerSlot < 0 ||
		 m_localPlayerSlot >= static_cast< int >(m_playerGridTrackers.size()) )
	{
		return false;
	}

	const GridDynamicTracker& tracker =
		m_playerGridTrackers[static_cast< size_t >(m_localPlayerSlot)];

	if ( !tracker.occupied )
		return false;

	int megaX = -1;
	int megaZ = -1;

	if ( !FineCellToMegaGridCell(tracker.prevCellX, tracker.prevCellZ, megaX, megaZ) )
		return false;

	return IsFineCellInsideMegaGridApproachZone(
		megaX,
		megaZ,
		tracker.prevCellX,
		tracker.prevCellZ
	);
#else
	return false;
#endif
}

bool CGameScene::IsLocalPlayer(const CGameObject* obj) const
{
    if (!obj) return false;
    auto* tag = obj->GetComponent<CActorTagComponent>();
    return tag && tag->kind == EActorKind::Player && tag->control == EPlayerControl::Local;
}

bool CGameScene::RollbackLocalPlayerMoveIfCollidingWorldStatic(const XMFLOAT3& previousPos)
{
	CGameObject* localPlayer = GetPlayer();
	if ( !localPlayer ) return false;
	if ( !m_Collision ) return false;

	auto* collider = localPlayer->GetComponent<CColliderComponent>();
	if ( !collider ) return false;

	const XMFLOAT3 currentPos = localPlayer->GetPosition();

	auto TestPositionAgainstWorldStatic = [ this, localPlayer, collider ] (const XMFLOAT3& testPos) -> bool
		{
			localPlayer->SetPosition(testPos);
			collider->OnUpdate(0.0f);
			return m_Collision->HasCollisionWithWorldStatic(collider);
		};

	// 현재 위치가 애초에 안 겹치면 아무 것도 안 함
	if ( !TestPositionAgainstWorldStatic(currentPos) )
		return false;

	// 후보 1: X만 롤백 (Z 이동은 살림)
	XMFLOAT3 candidateRollbackX = currentPos;
	candidateRollbackX.x = previousPos.x;

	// 후보 2: Z만 롤백 (X 이동은 살림)
	XMFLOAT3 candidateRollbackZ = currentPos;
	candidateRollbackZ.z = previousPos.z;

	const bool xResolved = !TestPositionAgainstWorldStatic(candidateRollbackX);
	const bool zResolved = !TestPositionAgainstWorldStatic(candidateRollbackZ);

	if ( xResolved && zResolved )
	{
		const float keepDistSqX =
			( candidateRollbackX.x - previousPos.x ) * ( candidateRollbackX.x - previousPos.x ) +
			( candidateRollbackX.z - previousPos.z ) * ( candidateRollbackX.z - previousPos.z );

		const float keepDistSqZ =
			( candidateRollbackZ.x - previousPos.x ) * ( candidateRollbackZ.x - previousPos.x ) +
			( candidateRollbackZ.z - previousPos.z ) * ( candidateRollbackZ.z - previousPos.z );

		const XMFLOAT3 resolvedPos = ( keepDistSqX >= keepDistSqZ ) ? candidateRollbackX : candidateRollbackZ;
		localPlayer->SetPosition(resolvedPos);
		collider->OnUpdate(0.0f);
		return true;
	}

	if ( xResolved )
	{
		localPlayer->SetPosition(candidateRollbackX);
		collider->OnUpdate(0.0f);
		return true;
	}

	if ( zResolved )
	{
		localPlayer->SetPosition(candidateRollbackZ);
		collider->OnUpdate(0.0f);
		return true;
	}

	// 둘 다 안 되면 전체 롤백
	localPlayer->SetPosition(previousPos);
	collider->OnUpdate(0.0f);
	return true;
}

bool CGameScene::OnProcessingMouseMessage(HWND /*hWnd*/, UINT msg, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	if ( msg == WM_LBUTTONDOWN )
	{
#ifndef USING_NETWORK
		RequestPlayerAttackBySlot(m_localPlayerSlot);
#endif
		return true;
	}
	return false;
}

bool CGameScene::OnProcessingKeyboardMessage(HWND /*hWnd*/, UINT msg, WPARAM wParam, LPARAM /*lParam*/)
{
	return false;
}

void CGameScene::RequestFireArrow(CGameObject* shooter, float speed, float lifeSec, float yOffset)
{
    if (!shooter) return;

    CGameObject* bowObj = nullptr;

    if (auto* equip = shooter->GetComponent<CPlayerEquipmentComponent>())
    {
        // 장비 컴포넌트가 있는 플레이어라면 Bow 장착시에만 발사 허용
        if (equip->GetEquippedWeapon() != EWeaponType::Bow)
            return;

        bowObj = equip->GetWeaponObject(EWeaponType::Bow);
    }

    // 방향/회전은 기존 로직 유지: shooter의 forward 사용
    const XMFLOAT4X4& W = shooter->GetWorldMatrix();
    XMFLOAT3 dir = { W._31, W._32, W._33 };

    XMVECTOR dirV = XMLoadFloat3(&dir);
    const float lenSq = XMVectorGetX(XMVector3LengthSq(dirV));
    if (lenSq < 1e-8f)
    {
        dir = XMFLOAT3(0.0f, 0.0f, 1.0f);
        dirV = XMLoadFloat3(&dir);
    }

    dirV = XMVector3Normalize(dirV);

    XMFLOAT3 dirN{};
    XMStoreFloat3(&dirN, dirV);

    // 시작 위치는 활 위치 사용, 없으면 기존 fallback
    XMFLOAT3 startPos{};
    if (bowObj)
    {
        startPos = bowObj->GetPosition();
    }
    else
    {
        startPos = shooter->GetPosition();
        startPos.y += yOffset;
    }

    const XMFLOAT3 vel =
    {
        dirN.x * speed,
        dirN.y * speed,
        dirN.z * speed
    };

    for (CGameObject* arrowObj : m_arrowRefs)
    {
        if (!arrowObj) continue;

        auto* arrow = arrowObj->GetComponent<CArrowComponent>();
        if (!arrow) continue;

        if (arrow->IsActive()) continue;

		arrow->Activate(startPos, vel, lifeSec, true);
		return;
    }
}

void CGameScene::RequestFireBullet(CGameObject* shooter, float speed, float lifeSec)
{
	if ( !shooter ) return;

	CGameObject* gunObj = nullptr;

	if ( auto* equip = shooter->GetComponent<CPlayerEquipmentComponent>() )
	{
		if ( equip->GetEquippedWeapon() != EWeaponType::Gun )
			return;

		gunObj = equip->GetWeaponObject(EWeaponType::Gun);
	}

	CGameObject* spawnSource = gunObj ? gunObj : shooter;
	CGameObject* directionSource = shooter;

	for ( CGameObject* bulletObj : m_bulletRefs )
	{
		if ( !bulletObj ) continue;

		auto* bullet = bulletObj->GetComponent<CBulletComponent>();
		if ( !bullet ) continue;
		if ( bullet->IsActive() ) continue;

		if ( bullet->FireFromObjects(spawnSource, directionSource, speed, lifeSec, true) )
		{
			return;
		}
	}
}

bool CGameScene::ProcessInput(UCHAR* /*pKeysBuffer*/)
{
    return false;
}

void CGameScene::AnimateObjects(float dt)
{
    // ------------------------------------------------------------------------
    // FrameSnapshot에서 좌표 업데이트
    // ------------------------------------------------------------------------

#ifdef USING_NETWORK
    DequeueNetworkMessage(NetworkMessageType::FrameState);
    if (std::holds_alternative<FrameSnapshot>(m_pendingNetworkMessage.data))
    {
        const FrameSnapshot& snapshot = std::get<FrameSnapshot>(m_pendingNetworkMessage.data);

        // Player 좌표 업데이트
        for (const auto& state : snapshot.players)
        {
            // id를 slot으로 사용 (0~3)
            int slot = static_cast<int>(state.id);
            CGameObject* player = GetPlayerBySlot(slot);
            if (!player) continue;


            // 로컬 플레이어는 서버 좌표로 덮어쓰지 않음 (선택적)
            // if (slot == m_localPlayerSlot) continue;

            player->SetPosition(state.position.x, state.position.y, state.position.z);

            // yaw 회전 적용
            if (auto* tr = player->GetComponent<CTransformComponent>())
            {
                tr->SetYawDegrees(state.yaw);
            }

            // 데모: animation state 강제 적용
            if (auto ac = player->GetAnimController())
            {
                const DecodedAnimStateCode decoded = DecodeStateCode(state.animation.stateCode);

                ac->SetMoveDirection(decoded.hasMove ? decoded.moveDirBits : 0u);
                ac->SetRunRequested(decoded.run);

                if (decoded.die)
                {
                    ac->SetAnimState(EAnimState::Die);
                }
                else if (decoded.hit)
                {
                    ac->RequestHit();
                    ac->SetAnimState(decoded.hasMove ? EAnimState::Move : EAnimState::Idle);
                }
                else if (decoded.roll)
                {
                    uint32_t rollDirBits = decoded.hasMove ? decoded.moveDirBits : DIR_FORWARD;
                    ac->RequestRoll(rollDirBits);
                    ac->SetAnimState(EAnimState::Attack);
                }
				else if ( decoded.attack )
				{
					ac->RequestAttack();
				}
                else
                {
                    ac->SetAnimState(decoded.hasMove ? EAnimState::Move : EAnimState::Idle);
                }
            }

            if (auto wc = player->GetComponent<CPlayerEquipmentComponent>())
            {
                wc->SetLoadout(state.weaponType);
            }


            if (slot == m_localPlayerSlot)
            {
                // 로컬 플레이어로 카메라 동기화
				auto pCamera = GetMainCamera();
                if (pCamera)
                {
                    XMFLOAT3 pos = player->GetPosition();
					pos.y += 1.7f; // 카메라 높이 보정 (플레이어 중심에서 약간 위)
                    pCamera->Update(pos, dt);
                    pCamera->SetLookAt(pos);
                    pCamera->RegenerateViewMatrix();
                }
            }
        }

        // Enemy 좌표 업데이트
        // skinnedObjects에서 NPC만 순회 (Fighter 제외)
        UINT enemyIndex = 0;
        const UINT totalEnemies = m_ghoulCount + m_swordManCount + m_bowManCount + m_MutantCount + m_bossCount;

        for (UINT j = 0; j < totalEnemies && j < (UINT)m_skinnedObjects.size(); ++j)
        {
            auto* obj = m_skinnedObjects[j].get();
            if (!obj) continue;

            auto* tag = obj->GetComponent<CActorTagComponent>();
            if (!tag || tag->kind != EActorKind::NPC) continue;

            if (enemyIndex < (UINT)snapshot.enemies.size())
            {
                const auto& state = snapshot.enemies[enemyIndex];
                obj->SetPosition(state.position.x, state.position.y, state.position.z);

                if (auto* tr = obj->GetComponent<CTransformComponent>())
                {
                    tr->SetYawDegrees(state.yaw);
                }

				if (auto* animComp = obj->GetComponent<CAnimatorComponent>())
				{
					if (auto* ctrl = animComp->EnsureMonsterController())
					{
						const DecodedAnimStateCode decoded = DecodeStateCode(state.animation.stateCode);

						EMonsterAnimState locomotionState = EMonsterAnimState::Idle;
						if (decoded.hasMove)
							locomotionState = decoded.run ? EMonsterAnimState::Run : EMonsterAnimState::Move;

						ctrl->SetLocomotionState(locomotionState);

						static std::unordered_map<uint64_t, uint32_t> s_prevEnemyStateCode;
						const uint32_t prevStateCode =
							(s_prevEnemyStateCode.find(state.id) != s_prevEnemyStateCode.end())
							? s_prevEnemyStateCode[state.id]
							: 0u;
						const DecodedAnimStateCode prevDecoded = DecodeStateCode(prevStateCode);

						if (decoded.die && !prevDecoded.die)
						{
							ctrl->RequestCommand(EMonsterAnimCommand::Death);
						}
						else if (decoded.hit && !prevDecoded.hit)
						{
							ctrl->RequestCommand(EMonsterAnimCommand::Hit);
						}
						else if (decoded.attack && !prevDecoded.attack)
						{
							ctrl->RequestCommand(EMonsterAnimCommand::Attack);
						}

						s_prevEnemyStateCode[state.id] = state.animation.stateCode;
						ctrl->Update(0.0f);
					}
				}
            }
            ++enemyIndex;
        }

		// Projectile 동기화
		size_t usedArrowCount = 0;
		size_t usedBulletCount = 0;

		for (const auto& b : snapshot.bullets)
		{
			const bool isArrow = (b.bulletType == 1u); // Protocol::BULLET_TYPE_ARROW

			if (isArrow)
			{
				if (usedArrowCount >= m_arrowRefs.size())
					continue;

				CGameObject* arrowObj = m_arrowRefs[usedArrowCount++];
				if (!arrowObj) continue;

				if ( auto* arrow = arrowObj->GetComponent<CArrowComponent>() )
				{
					arrow->Activate(b.position, b.velocity, 2.0f);
					auto arrowtransform = arrowObj->GetComponent<CTransformComponent>();
					arrowtransform->SetLookDirection(b.velocity);
				}
			}
			else
			{
				if (usedBulletCount >= m_bulletRefs.size())
					continue;

				CGameObject* bulletObj = m_bulletRefs[usedBulletCount++];
				if (!bulletObj) continue;

				if ( auto* bullet = bulletObj->GetComponent<CBulletComponent>() )
				{
					bullet->Activate(b.position, b.velocity, 2.0f, true);
				}
			}
		}

		for (size_t i = usedArrowCount; i < m_arrowRefs.size(); ++i)
		{
			if (!m_arrowRefs[i]) continue;
			if (auto* arrow = m_arrowRefs[i]->GetComponent<CArrowComponent>())
				arrow->Deactivate();
		}

		for (size_t i = usedBulletCount; i < m_bulletRefs.size(); ++i)
		{
			if (!m_bulletRefs[i]) continue;
			if (auto* bullet = m_bulletRefs[i]->GetComponent<CBulletComponent>())
				bullet->Deactivate();
		}
    }
#endif
   

    // ------------------------------------------------------------------------
    // 기존 애니메이션 로직
    // ------------------------------------------------------------------------
	CCamera* camera = GetMainCamera();

	for ( UINT j = 0; j < ( UINT ) m_skinnedObjects.size(); ++j )
	{
		if ( !m_skinnedObjects[j] ) continue;

		if ( camera && !m_skinnedObjects[j]->IsVisible(camera) )
			continue;

		m_skinnedObjects[j]->Animate(dt);
	}

    for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
    {
        if (!m_staticObjects[j]) continue;
        m_staticObjects[j]->Animate(dt);
    }
#ifndef USING_NETWORK
    UpdatePreparedBowArrows();
#endif

    CGameObject* local = GetPlayer();
    if (local && m_pPlayerSpotFollower && (m_pPlayerSpotFollower->GetTarget() == nullptr))
    {
        m_pPlayerSpotFollower->SetTarget(local);
    }

	for ( UINT j = 0; j < ( UINT ) m_lightObjects.size(); ++j )
	{
		if ( !m_lightObjects[j] ) continue;
		m_lightObjects[j]->Animate(dt);
	}

#ifndef USING_NETWORK
	UpdateDynamicGridState();
#endif
}

void CGameScene::CollisionObjects()
{
	if ( !m_Collision ) return;
	m_Collision->OnUpdate();
}

void CGameScene::UpdateShaderVariables(ID3D12GraphicsCommandList* /*cmd*/)
{
    if (m_pcbMappedLights)
    {
        ::ZeroMemory(m_pcbMappedLights, sizeof(LIGHTS));
		m_pcbMappedLights->m_xmf4GlobalAmbient = XMFLOAT4(0.20f, 0.20f, 0.20f, 1.0f);

        UINT li = 0;
        for (auto& obj : m_lightObjects)
        {
            if (!obj) continue;

            auto* lc = obj->GetComponent<CLightComponent>();
            if (!lc) continue;
            if (!lc->IsEnabled()) continue;
            if (li >= MAX_LIGHTS) break;

            lc->Fill(m_pcbMappedLights->m_pLights[li]);
            ++li;
        }
    }

    if (m_pcbMappedMaterials && m_pMaterials)
        ::memcpy(m_pcbMappedMaterials, m_pMaterials.get(), sizeof(MATERIALS));
	if ( m_pcbMappedFog )
		::memcpy(m_pcbMappedFog, &m_fogData, sizeof(CB_FOG));

    if (m_staticBatch.mappedGameObjects && !m_staticBatch.objectRefs.empty())
    {
        const UINT ncb = m_staticBatch.cbElementBytes;

        for (UINT j = 0; j < (UINT)m_staticBatch.objectRefs.size(); ++j)
        {
            auto* obj = m_staticBatch.objectRefs[j];
            if (!obj) continue;

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)m_staticBatch.mappedGameObjects + j * ncb);

            const XMFLOAT4X4& W = obj->GetWorldMatrix();

            XMStoreFloat4x4(
                &cb->m_xmf4x4World,
                XMMatrixTranspose(XMLoadFloat4x4(&W))
            );

            cb->m_nObjectID = j;
        }
    }

    if (m_skinnedBatch.mappedGameObjects && !m_skinnedBatch.objectRefs.empty())
    {
        const UINT ncb = m_skinnedBatch.cbElementBytes;

        for (UINT j = 0; j < (UINT)m_skinnedBatch.objectRefs.size(); ++j)
        {
            auto* obj = m_skinnedBatch.objectRefs[j];
            if (!obj) continue;

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)m_skinnedBatch.mappedGameObjects + j * ncb);

            const XMFLOAT4X4& W = obj->GetWorldMatrix();

            XMStoreFloat4x4(
                &cb->m_xmf4x4World,
                XMMatrixTranspose(XMLoadFloat4x4(&W))
            );

            cb->m_nObjectID = j;
        }
    }

	if ( m_colliderBatch.mappedGameObjects && !m_colliderBatch.objectRefs.empty() )
	{
		const UINT ncb = m_colliderBatch.cbElementBytes;

		for ( UINT j = 0; j < ( UINT ) m_colliderBatch.objectRefs.size(); ++j )
		{
			auto* obj = m_colliderBatch.objectRefs[j];
			if ( !obj ) continue;

			auto* cb = ( CB_GAMEOBJECT_INFO* ) ( ( UINT8* ) m_colliderBatch.mappedGameObjects + j * ncb );

			const XMFLOAT4X4& W = obj->GetWorldMatrix();

			XMStoreFloat4x4(
				&cb->m_xmf4x4World,
				XMMatrixTranspose(XMLoadFloat4x4(&W))
			);

			cb->m_nObjectID = j;
		}
	}
}

void CGameScene::OnPrepareRender(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
    CScene::OnPrepareRender(cmd, camera);

	if ( camera )
	{
		camera->UpdateBoundingFrustum();
		UpdateStaticWorldLodSelection(camera);
		UpdateSkinnedWorldLodSelection(camera);
	}

	UpdateShaderVariables(cmd);

    if (m_pd3dcbLights)
    {
        D3D12_GPU_VIRTUAL_ADDRESS lightsGpu = m_pd3dcbLights->GetGPUVirtualAddress();
        cmd->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_LIGHT, lightsGpu);
    }

    if (m_pd3dcbMaterials)
    {
        D3D12_GPU_VIRTUAL_ADDRESS matsGpu = m_pd3dcbMaterials->GetGPUVirtualAddress();
        cmd->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_MATERIAL, matsGpu);
    }
}

void CGameScene::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !m_bStartedGameplayMusic )
	{
		if ( m_pAudioManager )
		{
			if ( auto* music = m_pAudioManager->GetMusicDirector() )
			{
				music->RequestState(EMusicState::Gameplay, false);
				music->BeginPendingTransition();
			}
		}

		m_bStartedGameplayMusic = true;
	}

	if ( m_staticBatch.shader )
	{
		RenderStaticInstanceGroups(cmd, camera);
	}

    if ( m_skinnedBatch.shader )
	{
		m_skinnedBatch.shader->Render(cmd, camera, &m_skinnedBatch);
		RenderSkinnedInstanceGroups(cmd, camera);
	}
    
#ifndef USING_NETWORK
	if ( m_colliderBatch.shader )
	{
		m_colliderBatch.shader->Render(cmd, camera, &m_colliderBatch);
		for ( UINT j = 0; j < ( UINT ) m_colliderObjects.size(); ++j )
		{
			if ( !m_colliderObjects[j] ) continue;
			m_colliderObjects[j]->Render(cmd, camera);
		}
	}
#endif
#ifndef USING_NETWORK
	{
		const bool isInsideMegaGridCenter = IsLocalPlayerInsideMegaGridCenter();

		if ( isInsideMegaGridCenter != m_bWasLocalPlayerInsideMegaGridCenter )
		{
			if ( m_pAudioManager )
			{
				if ( auto* music = m_pAudioManager->GetMusicDirector() )
				{
					music->SetCrossFadeSeconds(1.0f);

					if ( isInsideMegaGridCenter )
					{
						OutputDebugStringA("[MegaGridBGM] local player entered center zone\n");
						music->RequestState(EMusicState::None, false);
						music->BeginPendingTransition();
					}
					else
					{
						OutputDebugStringA("[MegaGridBGM] local player left center zone\n");
						music->RequestState(EMusicState::Gameplay, false);
						music->BeginPendingTransition();
					}
				}
			}

			m_bWasLocalPlayerInsideMegaGridCenter = isInsideMegaGridCenter;
		}
	}
#endif

	RenderUI(cmd, camera);

    if (m_Collision)
    {
    }
}

void CGameScene::BuildObjectsCollider()
{
    m_Collision = make_unique<CCollisionSystem>();
    for (auto& obj : m_staticObjects)
    {
        m_Collision->RegisterCollider(obj->GetComponent<CColliderComponent>());
    }
    for (auto& obj : m_skinnedObjects)
    {
        m_Collision->RegisterCollider(obj->GetComponent<CColliderComponent>());
    }
}
