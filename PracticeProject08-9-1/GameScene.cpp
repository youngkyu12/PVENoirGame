//-----------------------------------------------------------------------------
// File: GameScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"
#include "GameSceneHelper.h"

using namespace GameSceneHelper;

CGameScene::CGameScene()
{
    m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };
    m_localPlayerSlot = 0;

	m_grassCount = 1;
    m_groundCount = 1;
    m_villagewallCount = 1;
	m_castleCount = 1;
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

	m_bSimulateLocalPlayerMonsterAttackCollision = true;
	m_bSimulateLocalAI = true;
	m_bSimulateLocalGhoulAI = true;
	m_bSimulateLocalBowManAI = true;
	m_bSimulateLocalSwordManAI = true;
	m_bSimulateLocalMutantAI = true;
	m_bSimulateLocalBossAI = true;

	m_bSimulateLocalMonsterChase = true;
	m_bPrevLocalMonsterChaseToggleKeyDown = false;
	m_bSimulateLocalEnemySpawner = true;
	m_bSimulateLocalPlayerWorldStaticRollback = true;
	m_bSimulateLocalTeleport = true;
	m_bSimulateLocalItemPickup = true;
	m_bCanBossStageDirectly = false;
	m_bSimulateLocalStageTeleport = true;
	m_bPrevLocalStageTeleportKeyDown.fill(false);

#ifdef USING_NETWORK
	m_bSimulateLocalPlayerMonsterAttackCollision = false;
	m_bSimulateLocalAI = false;

	m_bSimulateLocalGhoulAI = false;
	m_bSimulateLocalBowManAI = false;
	m_bSimulateLocalSwordManAI = false;
	m_bSimulateLocalMutantAI = false;
	m_bSimulateLocalBossAI = false;

	m_bSimulateLocalMonsterChase = false;
	m_bSimulateLocalEnemySpawner = false;
	m_bSimulateLocalPlayerWorldStaticRollback = false;
	m_bSimulateLocalTeleport = false;
	m_bSimulateLocalItemPickup = true;
	m_bCanBossStageDirectly = false;
	m_bSimulateLocalStageTeleport = false;
	m_bPrevLocalStageTeleportKeyDown.fill(false);
#endif

	m_bLocalPlayerDead = false;
	m_bLocalPlayerRespawnUsed = false;
	m_localPlayerRespawnTimer = 0.0f;

#ifdef USING_NETWORK
	m_prevPlayerNetworkStateCode.clear();
#endif
	m_deadMonsters.clear();

	m_bLocalPlayerInsideCastleCenterMegaGrid = false;
}

void CGameScene::SetFrameResourceIndex(UINT frameResourceIndex)
{
	m_nFrameResourceIndex = frameResourceIndex % kFrameResourceCount;

	m_depthFog.SetFrameResourceIndex(m_nFrameResourceIndex);
	m_shadowMap.SetFrameResourceIndex(m_nFrameResourceIndex);

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	if ( m_staticBatch.cbElementBytes > 0 &&
		 m_staticBatch.mappedGameObjects[frameIndex] &&
		 m_staticBatch.baseCbvGpu[frameIndex].ptr != 0 )
	{
		for ( UINT objectIndex = 0;
			  objectIndex < static_cast< UINT >(m_staticBatch.objectRefs.size());
			  ++objectIndex )
		{
			CGameObject* obj = m_staticBatch.objectRefs[objectIndex];

			if ( !obj )
				continue;

			CB_GAMEOBJECT_INFO* cb =
				reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >(m_staticBatch.mappedGameObjects[frameIndex]) +
					objectIndex * m_staticBatch.cbElementBytes
				);

			obj->SetCbvGPUDescriptorHandlePtr(
				m_staticBatch.baseCbvGpu[frameIndex].ptr +
				static_cast< UINT64 >( objectIndex ) * m_staticBatch.cbvInc
			);

			obj->SetMappedGameObjectCB(cb);
		}
	}

	if ( m_skinnedBatch.cbElementBytes > 0 &&
		 m_skinnedBatch.mappedGameObjects[frameIndex] &&
		 m_skinnedBatch.baseCbvGpu[frameIndex].ptr != 0 )
	{
		for ( UINT objectIndex = 0;
			  objectIndex < static_cast< UINT >(m_skinnedBatch.objectRefs.size());
			  ++objectIndex )
		{
			CGameObject* obj = m_skinnedBatch.objectRefs[objectIndex];

			if ( !obj )
				continue;

			CB_GAMEOBJECT_INFO* cb =
				reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >(m_skinnedBatch.mappedGameObjects[frameIndex]) +
					objectIndex * m_skinnedBatch.cbElementBytes
				);

			obj->SetCbvGPUDescriptorHandlePtr(
				m_skinnedBatch.baseCbvGpu[frameIndex].ptr +
				static_cast< UINT64 >( objectIndex ) * m_skinnedBatch.cbvInc
			);

			obj->SetMappedGameObjectCB(cb);
		}
	}

	if ( m_colliderBatch.cbElementBytes > 0 &&
		 m_colliderBatch.mappedGameObjects[frameIndex] &&
		 m_colliderBatch.baseCbvGpu[frameIndex].ptr != 0 )
	{
		for ( UINT objectIndex = 0;
			  objectIndex < static_cast< UINT >(m_colliderBatch.objectRefs.size());
			  ++objectIndex )
		{
			CGameObject* obj = m_colliderBatch.objectRefs[objectIndex];

			if ( !obj )
				continue;

			CB_GAMEOBJECT_INFO* cb =
				reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >(m_colliderBatch.mappedGameObjects[frameIndex]) +
					objectIndex * m_colliderBatch.cbElementBytes
				);

			obj->SetCbvGPUDescriptorHandlePtr(
				m_colliderBatch.baseCbvGpu[frameIndex].ptr +
				static_cast< UINT64 >( objectIndex ) * m_colliderBatch.cbvInc
			);

			obj->SetMappedGameObjectCB(cb);
		}
	}
}

void CGameScene::InitializeSpatialGrid()
{
	m_sceneGrid.Initialize();

	for ( auto& tracker : m_playerGridTrackers )
		tracker = GridDynamicTracker{};

	m_monsterGridTrackers.clear();
	m_arrowGridTrackers.clear();
	m_bulletGridTrackers.clear();
}

void CGameScene::ShutdownSpatialGrid()
{
	m_sceneGrid.Shutdown();

	for ( auto& tracker : m_playerGridTrackers )
		tracker = GridDynamicTracker{};

	m_monsterGridTrackers.clear();
	m_arrowGridTrackers.clear();
	m_bulletGridTrackers.clear();
}

bool CGameScene::TryGetTreeCullReferenceGridCell(
	CCamera* camera,
	int& outCellX,
	int& outCellZ,
	int& outMegaX,
	int& outMegaZ) const
{
	outCellX = -1;
	outCellZ = -1;
	outMegaX = -1;
	outMegaZ = -1;

	if ( !m_sceneGrid.IsInitialized() )
		return false;

	if ( camera )
	{
		const XMFLOAT3 cameraPosition = camera->GetPosition();

		if ( m_sceneGrid.WorldToCell(cameraPosition.x, cameraPosition.z, outCellX, outCellZ) )
		{
			if ( m_sceneGrid.FineCellToMegaGridCell(outCellX, outCellZ, outMegaX, outMegaZ) )
				return true;
		}
	}

	CGameObject* localPlayer = GetPlayer();

	if ( !localPlayer )
		localPlayer = GetPlayerBySlot(0);

	if ( !localPlayer )
		return false;

	const XMFLOAT3 playerPosition = localPlayer->GetPosition();

	if ( !m_sceneGrid.WorldToCell(playerPosition.x, playerPosition.z, outCellX, outCellZ) )
		return false;

	return m_sceneGrid.FineCellToMegaGridCell(outCellX, outCellZ, outMegaX, outMegaZ);
}

bool CGameScene::ShouldCullTreesByVillageGrid(CCamera* camera) const
{
	if ( !m_sceneGrid.IsInitialized() )
		return false;

	// Castle 포탈로 중앙 성 내부에 들어간 경우에는
	// 기존 문/성벽 시야 판정과 무관하게 모든 나무를 컬링한다.
	if ( m_bLocalPlayerInsideCastleCenterMegaGrid )
		return true;

	CGameObject* localPlayer = GetPlayer();

	if ( !localPlayer )
		localPlayer = GetPlayerBySlot(0);

	if ( localPlayer )
	{
		const XMFLOAT3 playerPosition = localPlayer->GetPosition();

		// Tower 포탈 등으로 위로 올라간 경우에는
		// 성벽/문 기준 나무 컬링을 하지 않는다.
		if ( playerPosition.y >= kDisableVillageTreeCullPlayerHeight )
			return false;
	}

	int cellX = -1;
	int cellZ = -1;
	int megaX = -1;
	int megaZ = -1;

	if ( !TryGetTreeCullReferenceGridCell(camera, cellX, cellZ, megaX, megaZ) )
		return false;

	return m_sceneGrid.ShouldCullTreesByVillageGridCell(megaX, megaZ, cellX, cellZ);
}

void CGameScene::AddDynamicCount(int cellX, int cellZ, EGridDynamicKind kind, int delta)
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	m_sceneGrid.AddDynamicCount(cellX, cellZ, kind, delta);
}

void CGameScene::RegisterStaticPlacementToGrid(const StaticPlacementEntry& placement, CGameObject* obj)
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	if ( !obj )
		return;

	const bool isBuilding =
		( placement.assetName == "VillageWall" ) ||
		( placement.assetName == "Castle" ) ||
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
				m_sceneGrid.StampBuildingCellsFromOOBB(subOOBB, touchedCells);
		}
	}

	if ( touchedCells.empty() )
	{
		int cellX = -1;
		int cellZ = -1;
		const XMFLOAT3 pos = obj->GetPosition();

		if ( m_sceneGrid.WorldToCell(pos.x, pos.z, cellX, cellZ) )
			touchedCells.insert(m_sceneGrid.GridCellIndex(cellX, cellZ));
	}

	m_sceneGrid.AddStaticTouchedCells(touchedCells);

	if ( IsTreeCullBlockerAssetName(placement.assetName) )
		m_sceneGrid.MarkTreeCullBlockerCells(touchedCells);
}

#ifndef USING_NETWORK
void CGameScene::RegisterTowerDoorPortal(CGameObject* tower)
{
	if ( !tower )
		return;

	auto* collider = tower->GetComponent<CColliderComponent>();
	if ( !collider )
		return;

	if ( collider->GetType() != EColliderType::OOBB )
		return;

	TowerDoorPortalEntry entry{};
	entry.tower = tower;
	entry.collider = collider;

	const auto& meshSets = collider->GetMeshOOBBSets();

	for ( size_t meshSetIndex = 0; meshSetIndex < meshSets.size(); ++meshSetIndex )
	{
		const MeshOOBBSet& set = meshSets[meshSetIndex];

		size_t ob = set.SubOOBBMetas.size();
		size_t obmin = set.LocalSubOOBBs.size();
		if ( ob < obmin ) obmin = ob;
		const size_t count = obmin;

		for ( size_t subIndex = 0; subIndex < count; ++subIndex )
		{
			const SubOOBBMeta& meta = set.SubOOBBMetas[subIndex];

			if ( kEnableTowerDoorPortalCollisionLog )
			{
				const bool isDoorA = IsTowerDoorFrame1Name(meta.meshName, meta.authoringPath);
				const bool isDoorB = IsTowerDoorFrame2Name(meta.meshName, meta.authoringPath);

				if ( isDoorA || isDoorB || kEnableTowerDoorPortalVerboseLog )
				{
					char buf[1024];
					sprintf_s(
						buf,
						"[TowerDoorPortal][REGISTER_SCAN] tower=%p meshSet=%zu sub=%zu mesh=\"%s\" path=\"%s\" isDoorA=%d isDoorB=%d\n",
						static_cast< void* >( tower ),
						meshSetIndex,
						subIndex,
						meta.meshName.c_str(),
						meta.authoringPath.c_str(),
						isDoorA ? 1 : 0,
						isDoorB ? 1 : 0
					);
					OutputDebugStringA(buf);
				}
			}

			TowerDoorSubBoxRef ref{};
			ref.meshSetIndex = meshSetIndex;
			ref.subIndex = subIndex;

			if ( IsTowerDoorFrame2Name(meta.meshName, meta.authoringPath) )
			{
				entry.doorBRefs.push_back(ref);
			}
			else if ( IsTowerDoorFrame1Name(meta.meshName, meta.authoringPath) )
			{
				entry.doorARefs.push_back(ref);
			}
		}
	}

	if ( entry.doorARefs.empty() || entry.doorBRefs.empty() )
	{
		if ( kEnableTowerDoorPortalCollisionLog )
		{
			char buf[512];
			sprintf_s(
				buf,
				"[TowerDoorPortal][REGISTER_FAILED] tower=%p meshSetCount=%zu doorABoxes=%zu doorBBoxes=%zu\n",
				static_cast< void* >( tower ),
				meshSets.size(),
				entry.doorARefs.size(),
				entry.doorBRefs.size()
			);
			OutputDebugStringA(buf);
		}
		return;
	}

	char buf[256];
	sprintf_s(
		buf,
		"[TowerDoorPortal][REGISTER] tower=%p doorABoxes=%zu doorBBoxes=%zu\n",
		static_cast< void* >( tower ),
		entry.doorARefs.size(),
		entry.doorBRefs.size()
	);
	OutputDebugStringA(buf);

	m_towerDoorPortals.push_back(std::move(entry));
}

void CGameScene::RegisterCastleDoorPortal(CGameObject* castle)
{
	if ( !castle )
		return;

	auto* collider = castle->GetComponent<CColliderComponent>();
	if ( !collider )
		return;

	if ( collider->GetType() != EColliderType::OOBB )
		return;

	CastleDoorPortalEntry entry{};
	entry.castle = castle;
	entry.collider = collider;

	const auto& meshSets = collider->GetMeshOOBBSets();

	for ( size_t meshSetIndex = 0; meshSetIndex < meshSets.size(); ++meshSetIndex )
	{
		const MeshOOBBSet& set = meshSets[meshSetIndex];

		size_t count = set.SubOOBBMetas.size();

		if ( set.LocalSubOOBBs.size() < count )
			count = set.LocalSubOOBBs.size();

		for ( size_t subIndex = 0; subIndex < count; ++subIndex )
		{
			const SubOOBBMeta& meta = set.SubOOBBMetas[subIndex];

			const int doorIndex = GetCastleDoorFrameIndexFromMeshName(meta.meshName);

			if ( doorIndex < 0 || doorIndex >= 8 )
				continue;

			DoorPortalSubBoxRef ref{};
			ref.meshSetIndex = meshSetIndex;
			ref.subIndex = subIndex;

			entry.doorRefsByIndex[( size_t ) doorIndex].push_back(ref);

			if ( kEnableTowerDoorPortalCollisionLog )
			{
				char buf[1024];
				sprintf_s(
					buf,
					"[CastleDoorPortal][REGISTER_SCAN] castle=%p meshSet=%zu sub=%zu mesh=\"%s\" path=\"%s\" doorIndex=%d doorName=\"%s\"\n",
					static_cast< void* >( castle ),
					meshSetIndex,
					subIndex,
					meta.meshName.c_str(),
					meta.authoringPath.c_str(),
					doorIndex,
					GetCastleDoorFrameDebugName(doorIndex)
				);
				OutputDebugStringA(buf);
			}
		}
	}

	auto AddPair =
		[ &entry ] (int sourceIndex, int targetIndex)
		{
			if ( sourceIndex < 0 || sourceIndex >= 8 )
				return;

			if ( targetIndex < 0 || targetIndex >= 8 )
				return;

			if ( entry.doorRefsByIndex[( size_t ) sourceIndex].empty() )
				return;

			if ( entry.doorRefsByIndex[( size_t ) targetIndex].empty() )
				return;

			CastleDoorPortalPair pair{};
			pair.sourceDoorIndex = sourceIndex;
			pair.targetDoorIndex = targetIndex;
			pair.sourceRefs = entry.doorRefsByIndex[( size_t ) sourceIndex];
			pair.targetRefs = entry.doorRefsByIndex[( size_t ) targetIndex];

			entry.pairs.push_back(std::move(pair));
		};

	// Unity 기준:
	// Double Door Frame (1) -> Double Door Frame 
	// Double Door Frame (2) -> Double Door Frame (4)
	// Double Door Frame (3) -> Double Door Frame (5)
	// Double Door Frame (7) -> Double Door Frame (6)
	AddPair(1, 0);
	AddPair(2, 4);
	AddPair(3, 5);
	AddPair(7, 6);

	if ( entry.pairs.empty() )
	{
		if ( kEnableTowerDoorPortalCollisionLog )
		{
			char buf[512];
			sprintf_s(
				buf,
				"[CastleDoorPortal][REGISTER_FAILED] castle=%p meshSetCount=%zu doorCounts=[%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu]\n",
				static_cast< void* >( castle ),
				meshSets.size(),
				entry.doorRefsByIndex[0].size(),
				entry.doorRefsByIndex[1].size(),
				entry.doorRefsByIndex[2].size(),
				entry.doorRefsByIndex[3].size(),
				entry.doorRefsByIndex[4].size(),
				entry.doorRefsByIndex[5].size(),
				entry.doorRefsByIndex[6].size(),
				entry.doorRefsByIndex[7].size()
			);
			OutputDebugStringA(buf);
		}
		return;
	}

	if ( kEnableTowerDoorPortalCollisionLog )
	{
		char buf[512];
		sprintf_s(
			buf,
			"[CastleDoorPortal][REGISTER] castle=%p pairCount=%zu doorCounts=[%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu]\n",
			static_cast< void* >( castle ),
			entry.pairs.size(),
			entry.doorRefsByIndex[0].size(),
			entry.doorRefsByIndex[1].size(),
			entry.doorRefsByIndex[2].size(),
			entry.doorRefsByIndex[3].size(),
			entry.doorRefsByIndex[4].size(),
			entry.doorRefsByIndex[5].size(),
			entry.doorRefsByIndex[6].size(),
			entry.doorRefsByIndex[7].size()
		);
		OutputDebugStringA(buf);
	}

	m_castleDoorPortals.push_back(std::move(entry));
}

void CGameScene::TickTowerDoorPortalCooldowns()
{
	for ( TowerDoorPortalEntry& portal : m_towerDoorPortals )
	{
		if ( portal.cooldownFrames > 0 )
			--portal.cooldownFrames;
	}

	for ( CastleDoorPortalEntry& portal : m_castleDoorPortals )
	{
		if ( portal.cooldownFrames > 0 )
			--portal.cooldownFrames;
	}
}

bool CGameScene::IsTowerDoorPortalOnCooldown() const
{
	for ( const TowerDoorPortalEntry& portal : m_towerDoorPortals )
	{
		if ( portal.cooldownFrames > 0 )
			return true;
	}

	for ( const CastleDoorPortalEntry& portal : m_castleDoorPortals )
	{
		if ( portal.cooldownFrames > 0 )
			return true;
	}

	return false;
}

int CGameScene::CountClearedMegaGrids() const
{
	int clearedCount = 0;

	for ( int megaNumber = 1; megaNumber <= CSceneGrid::kMegaGridCount; ++megaNumber )
	{
		const int zeroBased = megaNumber - 1;
		const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
		const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

		if ( m_sceneGrid.IsMegaGridCleared(megaX, megaZ) )
			++clearedCount;
	}

	return clearedCount;
}

bool CGameScene::CanUseCastleDoorPortal() const
{
	if ( m_bCanBossStageDirectly )
		return true;
	return CountClearedMegaGrids() >= kRequiredClearedMegaGridCountForCastlePortal;
}

bool CGameScene::TryTeleportLocalPlayerByTowerDoorPortal(bool forceLog)
{
	const bool shouldLog = kEnableTowerDoorPortalCollisionLog && forceLog;

	if ( shouldLog )
	{
		char buf[256];
		sprintf_s(
			buf,
			"[TowerDoorPortal][PROBE_BEGIN] portalCount=%zu localDead=%d\n",
			m_towerDoorPortals.size(),
			m_bLocalPlayerDead ? 1 : 0
		);
		OutputDebugStringA(buf);
	}

	if ( m_bLocalPlayerDead )
	{
		if ( shouldLog )
			DebugPrintTowerDoorPortalLine("[TowerDoorPortal][PROBE_ABORT] local player is dead");
		return false;
	}

	CGameObject* player = GetPlayer();
	if ( !player )
	{
		if ( shouldLog )
			DebugPrintTowerDoorPortalLine("[TowerDoorPortal][PROBE_ABORT] GetPlayer returned null");
		return false;
	}

	auto* playerCollider = player->GetComponent<CColliderComponent>();
	if ( !playerCollider )
	{
		if ( shouldLog )
			DebugPrintTowerDoorPortalLine("[TowerDoorPortal][PROBE_ABORT] player collider missing");
		return false;
	}

	if ( playerCollider->GetType() != EColliderType::BCapsule )
	{
		if ( shouldLog )
			DebugPrintTowerDoorPortalLine("[TowerDoorPortal][PROBE_ABORT] player collider is not BCapsule");
		return false;
	}

	if ( !playerCollider->IsCollisionEnabled() )
	{
		if ( shouldLog )
			DebugPrintTowerDoorPortalLine("[TowerDoorPortal][PROBE_ABORT] player collider disabled");
		return false;
	}

	playerCollider->UpdateWorldBounds();

	const BoundingCapsule playerCapsule = playerCollider->GetBCapsule();
	const XMFLOAT3 playerPos = player->GetPosition();

	auto GetWorldBox =
		[ ](
			const TowerDoorPortalEntry& portal,
			const TowerDoorSubBoxRef& ref
		) -> const BoundingOrientedBox*
		{
			if ( !portal.collider )
				return nullptr;

			const auto& sets = portal.collider->GetMeshOOBBSets();

			if ( ref.meshSetIndex >= sets.size() )
				return nullptr;

			const MeshOOBBSet& set = sets[ref.meshSetIndex];

			if ( ref.subIndex >= set.WorldSubOOBBs.size() )
				return nullptr;

			return &set.WorldSubOOBBs[ref.subIndex];
		};

	auto DoesDoorGroupIntersect =
		[ & ](
			const TowerDoorPortalEntry& portal,
			const std::vector<TowerDoorSubBoxRef>& refs,
			const char* doorName
		) -> bool
		{
			bool anyHit = false;

			for ( const TowerDoorSubBoxRef& ref : refs )
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if ( !box )
				{
					if ( shouldLog )
					{
						char buf[256];
						sprintf_s(
							buf,
							"[TowerDoorPortal][BOX_MISSING] door=%s meshSet=%zu sub=%zu\n",
							doorName,
							ref.meshSetIndex,
							ref.subIndex
						);
						OutputDebugStringA(buf);
					}
					continue;
				}

				const bool hit = playerCapsule.Intersects(*box);

				if ( shouldLog && ( hit || kEnableTowerDoorPortalVerboseLog ) )
				{
					char buf[512];
					sprintf_s(
						buf,
						"[TowerDoorPortal][DOOR_TEST] door=%s hit=%d meshSet=%zu sub=%zu playerPos=(%.3f, %.3f, %.3f) boxCenter=(%.3f, %.3f, %.3f) boxExtents=(%.3f, %.3f, %.3f)\n",
						doorName,
						hit ? 1 : 0,
						ref.meshSetIndex,
						ref.subIndex,
						playerPos.x,
						playerPos.y,
						playerPos.z,
						box->Center.x,
						box->Center.y,
						box->Center.z,
						box->Extents.x,
						box->Extents.y,
						box->Extents.z
					);
					OutputDebugStringA(buf);
				}

				if ( hit )
					anyHit = true;
			}

			return anyHit;
		};

	auto ComputeDoorGroupCenter =
		[ & ](
			const TowerDoorPortalEntry& portal,
			const std::vector<TowerDoorSubBoxRef>& refs,
			XMFLOAT3& outCenter
		) -> bool
		{
			XMVECTOR sum = XMVectorZero();
			int count = 0;

			for ( const TowerDoorSubBoxRef& ref : refs )
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if ( !box )
					continue;

				sum += XMLoadFloat3(&box->Center);
				++count;
			}

			if ( count <= 0 )
				return false;

			sum = XMVectorScale(sum, 1.0f / static_cast< float >( count ));
			XMStoreFloat3(&outCenter, sum);
			return true;
		};

	auto ComputeDoorGroupBottomY =
		[ & ](
			const TowerDoorPortalEntry& portal,
			const std::vector<TowerDoorSubBoxRef>& refs,
			float& outBottomY
		) -> bool
		{
			bool found = false;
			float bottomY = FLT_MAX;

			for ( const TowerDoorSubBoxRef& ref : refs )
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if ( !box )
					continue;

				XMFLOAT3 corners[BoundingOrientedBox::CORNER_COUNT];
				box->GetCorners(corners);

				for ( const XMFLOAT3& corner : corners )
				{
					if ( !found || corner.y < bottomY )
					{
						bottomY = corner.y;
						found = true;
					}
				}
			}

			if ( !found )
				return false;

			outBottomY = bottomY;
			return true;
		};

	auto GetFirstDoorBox =
		[ & ](
			const TowerDoorPortalEntry& portal,
			const std::vector<TowerDoorSubBoxRef>& refs
		) -> const BoundingOrientedBox*
		{
			if ( refs.empty() )
				return nullptr;

			return GetWorldBox(portal, refs.front());
		};

	auto ComputeDoorHorizontalNormal =
		[ & ](
			const BoundingOrientedBox& box,
			XMVECTOR& outNormal
		) -> bool
		{
			// Door frame은 일반적으로 local Z가 두께 방향이다.
			// 그래도 혹시 local X가 더 얇은 경우까지 대비해서,
			// X/Z 중 더 얇은 축을 문의 법선 후보로 쓴다.
			XMVECTOR localAxis = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			if ( box.Extents.x < box.Extents.z )
				localAxis = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

			XMVECTOR q = XMLoadFloat4(&box.Orientation);
			XMVECTOR n = XMVector3Rotate(localAxis, q);

			// 출구 방향은 수평 방향만 사용한다.
			n = XMVectorSetY(n, 0.0f);

			const float lenSq = XMVectorGetX(XMVector3LengthSq(n));
			if ( lenSq <= 1.0e-6f )
				return false;

			outNormal = XMVector3Normalize(n);
			return true;
		};

	auto RotateLocalPlayerAndCameraForPortal =
		[&](
			float yawDeltaDeg,
			float& outOldCameraYaw,
			float& outNewCameraYaw,
			float& outOldPlayerYaw,
			float& outNewPlayerYaw
		) -> bool
		{
			outOldCameraYaw = 0.0f;
			outNewCameraYaw = 0.0f;
			outOldPlayerYaw = 0.0f;
			outNewPlayerYaw = 0.0f;

			bool rotatedAny = false;

			CCamera* camera = GetMainCamera();

			// 1) 카메라 yaw를 먼저 돌린다.
			if ( camera )
			{
				outOldCameraYaw = camera->GetYaw();
				outNewCameraYaw = NormalizeYawDegrees180(outOldCameraYaw + yawDeltaDeg);

				camera->GetYaw() = outNewCameraYaw;

				XMFLOAT3 cameraTarget = player->GetPosition();
				cameraTarget.y += 1.7f;

				camera->Update(cameraTarget, 0.0f);
				camera->SetLookAt(cameraTarget);
				camera->RegenerateViewMatrix();

				rotatedAny = true;
			}

			// 2) 로컬 플레이어의 실제 회전 기준은 CPlayerControllerComponent 쪽이다.
			//    프레임워크 입력 처리도 pc->SetYawDegrees(cameraYawDeg),
			//    pc->RotateTowardYawDegrees(cameraYawDeg, ...)를 사용하므로
			//    포탈 직후에도 controller yaw를 카메라 yaw와 같은 값으로 맞춘다.
			const float desiredPlayerYaw =
				camera
				? outNewCameraYaw
				: NormalizeYawDegrees180(outOldPlayerYaw + yawDeltaDeg);

			if ( auto* tr = player->GetComponent<CTransformComponent>() )
			{
				outOldPlayerYaw = QuaternionToYawDegrees(tr->rotation);
			}

			outNewPlayerYaw = desiredPlayerYaw;

			if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
			{
				controller->SetYawDegrees(outNewPlayerYaw);
				controller->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
				controller->SetInputDirection(static_cast< DWORD >( 0 ));
				controller->SetRunRequested(false);
				rotatedAny = true;
			}
			else if ( auto* tr = player->GetComponent<CTransformComponent>() )
			{
				tr->SetYawDegrees(outNewPlayerYaw);
				rotatedAny = true;
			}
			else
			{
				player->Rotate(0.0f, yawDeltaDeg, 0.0f);
				rotatedAny = true;
			}

			return rotatedAny;
		};

	auto TeleportBetweenDoorGroups =
		[&](
			TowerDoorPortalEntry& portal,
			const std::vector<TowerDoorSubBoxRef>& sourceRefs,
			const std::vector<TowerDoorSubBoxRef>& targetRefs,
			const char* debugFrom,
			const char* debugTo
		) -> bool
		{
			XMFLOAT3 sourceCenter{};
			XMFLOAT3 targetCenter{};

			if ( !ComputeDoorGroupCenter(portal, sourceRefs, sourceCenter) )
				return false;

			if ( !ComputeDoorGroupCenter(portal, targetRefs, targetCenter) )
				return false;

			XMVECTOR sourceV = XMLoadFloat3(&sourceCenter);
			XMVECTOR targetV = XMLoadFloat3(&targetCenter);

			const BoundingOrientedBox* sourceBox = GetFirstDoorBox(portal, sourceRefs);
			const BoundingOrientedBox* targetBox = GetFirstDoorBox(portal, targetRefs);

			XMVECTOR sourceNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			XMVECTOR targetNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			bool hasSourceNormal = false;
			bool hasTargetNormal = false;

			if ( sourceBox )
				hasSourceNormal = ComputeDoorHorizontalNormal(*sourceBox, sourceNormal);

			if ( targetBox )
				hasTargetNormal = ComputeDoorHorizontalNormal(*targetBox, targetNormal);

			// 플레이어가 source 문 기준 어느 면에서 들어왔는지 구한다.
			// 양쪽 문이 같은 방향을 보고 있으면, target에서도 같은 면으로 내보내야 한다.
			float sideSign = 1.0f;

			if ( hasSourceNormal )
			{
				XMVECTOR playerV = XMLoadFloat3(&playerPos);
				XMVECTOR sourceToPlayer = playerV - sourceV;
				sourceToPlayer = XMVectorSetY(sourceToPlayer, 0.0f);

				const float sideDot = XMVectorGetX(XMVector3Dot(sourceToPlayer, sourceNormal));
				sideSign = ( sideDot >= 0.0f ) ? 1.0f : -1.0f;
			}

			XMVECTOR exitDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			if ( hasTargetNormal )
			{
				exitDir = XMVectorScale(targetNormal, sideSign);
			}
			else
			{
				// fallback: 기존 방식
				exitDir = targetV - sourceV;
				exitDir = XMVectorSetY(exitDir, 0.0f);

				const float lenSq = XMVectorGetX(XMVector3LengthSq(exitDir));
				if ( lenSq > 1.0e-6f )
					exitDir = XMVector3Normalize(exitDir);
				else
					exitDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			}

			XMVECTOR dstV = targetV + XMVectorScale(exitDir, kTowerDoorPortalExitOffset);

			XMFLOAT3 dst{};
			XMStoreFloat3(&dst, dstV);

			float targetBottomY = 0.0f;
			float appliedYOffset = 0.0f;

			if ( ComputeDoorGroupBottomY(portal, targetRefs, targetBottomY) )
			{
				appliedYOffset =
					( targetBottomY > kTowerDoorPortalUpperHeightThreshold )
					? kTowerDoorPortalUpperExitYOffset
					: kTowerDoorPortalLowerExitYOffset;

				dst.y = targetBottomY + appliedYOffset;
			}
			else
			{
				dst.y = playerPos.y;
			}

			player->SetPosition(dst);

			float oldCameraYaw = 0.0f;
			float newCameraYaw = 0.0f;
			float oldPlayerYaw = 0.0f;
			float newPlayerYaw = 0.0f;

			const bool rotatedPortalView =
				RotateLocalPlayerAndCameraForPortal(
					180.0f,
					oldCameraYaw,
					newCameraYaw,
					oldPlayerYaw,
					newPlayerYaw
				);

			playerCollider->UpdateWorldBounds();

			if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
			{
				controller->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
				controller->SetInputDirection(static_cast< DWORD >( 0 ));
				controller->SetRunRequested(false);
			}

			portal.cooldownFrames = kTowerDoorPortalCooldownFrames;

			UpdateDynamicGridState();

			return true;
		};

	for ( TowerDoorPortalEntry& portal : m_towerDoorPortals )
	{
		if ( portal.cooldownFrames > 0 )
		{
			if ( shouldLog )
			{
				char buf[256];
				sprintf_s(
					buf,
					"[TowerDoorPortal][SKIP_COOLDOWN] tower=%p cooldownFrames=%d\n",
					static_cast< void* >( portal.tower ),
					portal.cooldownFrames
				);
				OutputDebugStringA(buf);
			}
			continue;
		}

		if ( !portal.collider )
		{
			if ( shouldLog )
				DebugPrintTowerDoorPortalLine("[TowerDoorPortal][SKIP] portal collider is null");
			continue;
		}

		const bool hitDoorA = DoesDoorGroupIntersect(
			portal,
			portal.doorARefs,
			"Double Door Frame"
		);

		const bool hitDoorB = DoesDoorGroupIntersect(
			portal,
			portal.doorBRefs,
			"Double Door Frame2"
		);

		if ( shouldLog )
		{
			char buf[512];
			sprintf_s(
				buf,
				"[TowerDoorPortal][PROBE_RESULT] tower=%p doorARefs=%zu doorBRefs=%zu hitA=%d hitB=%d\n",
				static_cast< void* >( portal.tower ),
				portal.doorARefs.size(),
				portal.doorBRefs.size(),
				hitDoorA ? 1 : 0,
				hitDoorB ? 1 : 0
			);
			OutputDebugStringA(buf);
		}

		// 양쪽이 동시에 맞으면 문 중앙/겹침 상태일 수 있으므로 이번 프레임은 무시.
		if ( hitDoorA && hitDoorB )
		{
			if ( shouldLog )
				DebugPrintTowerDoorPortalLine("[TowerDoorPortal][SKIP_BOTH_HIT] both door groups intersected");
			continue;
		}

		if ( hitDoorA )
		{
			if ( TeleportBetweenDoorGroups(
				portal,
				portal.doorARefs,
				portal.doorBRefs,
				"Double Door Frame",
				"Double Door Frame2") )
			{
				return true;
			}
		}
		else if ( hitDoorB )
		{
			if ( TeleportBetweenDoorGroups(
				portal,
				portal.doorBRefs,
				portal.doorARefs,
				"Double Door Frame2",
				"Double Door Frame") )
			{
				return true;
			}
		}
	}

	if ( shouldLog )
		DebugPrintTowerDoorPortalLine("[TowerDoorPortal][NO_DOOR_HIT] no registered door OOBB intersected player capsule");

	return false;
}

bool CGameScene::TryTeleportLocalPlayerByCastleDoorPortal(bool forceLog)
{
	const bool shouldLog = kEnableTowerDoorPortalCollisionLog && forceLog;

	if ( !CanUseCastleDoorPortal() )
	{
		if ( shouldLog )
		{
			char buf[256];
			sprintf_s(
				buf,
				"[CastleDoorPortal][LOCKED] clearedMegaGrids=%d required=%d\n",
				CountClearedMegaGrids(),
				kRequiredClearedMegaGridCountForCastlePortal
			);
			OutputDebugStringA(buf);
		}

		return false;
	}

	if ( m_bLocalPlayerDead )
		return false;

	CGameObject* player = GetPlayer();
	if ( !player )
		return false;

	auto* playerCollider = player->GetComponent<CColliderComponent>();
	if ( !playerCollider )
		return false;

	if ( playerCollider->GetType() != EColliderType::BCapsule )
		return false;

	if ( !playerCollider->IsCollisionEnabled() )
		return false;

	playerCollider->UpdateWorldBounds();

	const BoundingCapsule playerCapsule = playerCollider->GetBCapsule();
	const XMFLOAT3 playerPos = player->GetPosition();

	auto GetWorldBox =
		[ ](
			const CastleDoorPortalEntry& portal,
			const DoorPortalSubBoxRef& ref
		) -> const BoundingOrientedBox*
		{
			if ( !portal.collider )
				return nullptr;

			const auto& sets = portal.collider->GetMeshOOBBSets();

			if ( ref.meshSetIndex >= sets.size() )
				return nullptr;

			const MeshOOBBSet& set = sets[ref.meshSetIndex];

			if ( ref.subIndex >= set.WorldSubOOBBs.size() )
				return nullptr;

			return &set.WorldSubOOBBs[ref.subIndex];
		};

	auto DoesDoorGroupIntersect =
		[ & ](
			const CastleDoorPortalEntry& portal,
			const std::vector<DoorPortalSubBoxRef>& refs,
			int doorIndex
		) -> bool
		{
			bool anyHit = false;

			for ( const DoorPortalSubBoxRef& ref : refs )
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);

				if ( !box )
					continue;

				const bool hit = playerCapsule.Intersects(*box);

				if ( shouldLog && ( hit || kEnableTowerDoorPortalVerboseLog ) )
				{
					char buf[512];
					sprintf_s(
						buf,
						"[CastleDoorPortal][DOOR_TEST] door=\"%s\" index=%d hit=%d meshSet=%zu sub=%zu playerPos=(%.3f, %.3f, %.3f) boxCenter=(%.3f, %.3f, %.3f) boxExtents=(%.3f, %.3f, %.3f)\n",
						GetCastleDoorFrameDebugName(doorIndex),
						doorIndex,
						hit ? 1 : 0,
						ref.meshSetIndex,
						ref.subIndex,
						playerPos.x,
						playerPos.y,
						playerPos.z,
						box->Center.x,
						box->Center.y,
						box->Center.z,
						box->Extents.x,
						box->Extents.y,
						box->Extents.z
					);
					OutputDebugStringA(buf);
				}

				if ( hit )
					anyHit = true;
			}

			return anyHit;
		};

	auto ComputeDoorGroupCenter =
		[ & ](
			const CastleDoorPortalEntry& portal,
			const std::vector<DoorPortalSubBoxRef>& refs,
			XMFLOAT3& outCenter
		) -> bool
		{
			XMVECTOR sum = XMVectorZero();
			int count = 0;

			for ( const DoorPortalSubBoxRef& ref : refs )
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);

				if ( !box )
					continue;

				sum += XMLoadFloat3(&box->Center);
				++count;
			}

			if ( count <= 0 )
				return false;

			sum = XMVectorScale(sum, 1.0f / static_cast< float >( count ));
			XMStoreFloat3(&outCenter, sum);
			return true;
		};

	auto GetFirstDoorBox =
		[ & ](
			const CastleDoorPortalEntry& portal,
			const std::vector<DoorPortalSubBoxRef>& refs
		) -> const BoundingOrientedBox*
		{
			if ( refs.empty() )
				return nullptr;

			return GetWorldBox(portal, refs.front());
		};

	auto ComputeDoorHorizontalNormal =
		[ ](
			const BoundingOrientedBox& box,
			XMVECTOR& outNormal
		) -> bool
		{
			XMVECTOR localAxis = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			if ( box.Extents.x < box.Extents.z )
				localAxis = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

			XMVECTOR q = XMLoadFloat4(&box.Orientation);
			XMVECTOR n = XMVector3Rotate(localAxis, q);
			n = XMVectorSetY(n, 0.0f);

			const float lenSq = XMVectorGetX(XMVector3LengthSq(n));

			if ( lenSq <= 1.0e-6f )
				return false;

			outNormal = XMVector3Normalize(n);
			return true;
		};

	auto TeleportCastleDoorPair =
		[ & ](
			CastleDoorPortalEntry& portal,
			const CastleDoorPortalPair& pair
		) -> bool
		{
			XMFLOAT3 sourceCenter{};
			XMFLOAT3 targetCenter{};

			if ( !ComputeDoorGroupCenter(portal, pair.sourceRefs, sourceCenter) )
				return false;

			if ( !ComputeDoorGroupCenter(portal, pair.targetRefs, targetCenter) )
				return false;

			const BoundingOrientedBox* sourceBox =
				GetFirstDoorBox(portal, pair.sourceRefs);

			const BoundingOrientedBox* targetBox =
				GetFirstDoorBox(portal, pair.targetRefs);

			XMVECTOR sourceV = XMLoadFloat3(&sourceCenter);
			XMVECTOR targetV = XMLoadFloat3(&targetCenter);

			XMVECTOR sourceNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			XMVECTOR targetNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			const bool hasSourceNormal =
				sourceBox && ComputeDoorHorizontalNormal(*sourceBox, sourceNormal);

			const bool hasTargetNormal =
				targetBox && ComputeDoorHorizontalNormal(*targetBox, targetNormal);

			float sideSign = 1.0f;

			if ( hasSourceNormal )
			{
				XMVECTOR playerV = XMLoadFloat3(&playerPos);
				XMVECTOR sourceToPlayer = playerV - sourceV;
				sourceToPlayer = XMVectorSetY(sourceToPlayer, 0.0f);

				const float sideDot =
					XMVectorGetX(XMVector3Dot(sourceToPlayer, sourceNormal));

				sideSign = ( sideDot >= 0.0f ) ? 1.0f : -1.0f;
			}

			XMVECTOR exitDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			if ( hasTargetNormal )
			{
				// Castle 문은 방향이 서로 반대이므로,
				// Tower처럼 카메라/플레이어를 돌리지 않고 같은 world 진행 방향으로 나오게 둔다.
				exitDir = XMVectorScale(targetNormal, sideSign);
			}
			else
			{
				exitDir = targetV - sourceV;
				exitDir = XMVectorSetY(exitDir, 0.0f);

				const float lenSq = XMVectorGetX(XMVector3LengthSq(exitDir));

				if ( lenSq > 1.0e-6f )
					exitDir = XMVector3Normalize(exitDir);
				else
					exitDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			}

			XMVECTOR dstV =
				targetV + XMVectorScale(exitDir, kCastleDoorPortalExitOffset);

			XMFLOAT3 dst{};
			XMStoreFloat3(&dst, dstV);

			// Castle 포탈은 같은 높이끼리 이동한다.
			// 문 OOBB center/bottom y를 쓰지 않고 현재 플레이어 높이를 유지한다.
			dst.y = playerPos.y;

			player->SetPosition(dst);
			playerCollider->UpdateWorldBounds();

			if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
			{
				controller->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
				controller->SetInputDirection(static_cast< DWORD >( 0 ));
				controller->SetRunRequested(false);
			}

			portal.cooldownFrames = kCastleDoorPortalCooldownFrames;

			UpdateDynamicGridState();
			MarkLocalPlayerEnteredCastleCenterMegaGrid();

			if ( shouldLog )
			{
				XMFLOAT3 exitDirF{};
				XMStoreFloat3(&exitDirF, exitDir);

				char buf[768];
				sprintf_s(
					buf,
					"[CastleDoorPortal][TELEPORT] %s -> %s dst=(%.3f, %.3f, %.3f) targetCenter=(%.3f, %.3f, %.3f) exitDir=(%.3f, %.3f, %.3f) sideSign=%.1f\n",
					GetCastleDoorFrameDebugName(pair.sourceDoorIndex),
					GetCastleDoorFrameDebugName(pair.targetDoorIndex),
					dst.x,
					dst.y,
					dst.z,
					targetCenter.x,
					targetCenter.y,
					targetCenter.z,
					exitDirF.x,
					exitDirF.y,
					exitDirF.z,
					sideSign
				);
				OutputDebugStringA(buf);
			}

			return true;
		};

	if ( shouldLog )
	{
		char buf[256];
		sprintf_s(
			buf,
			"[CastleDoorPortal][PROBE_BEGIN] portalCount=%zu localDead=%d\n",
			m_castleDoorPortals.size(),
			m_bLocalPlayerDead ? 1 : 0
		);
		OutputDebugStringA(buf);
	}

	for ( CastleDoorPortalEntry& portal : m_castleDoorPortals )
	{
		if ( portal.cooldownFrames > 0 )
		{
			if ( shouldLog )
			{
				char buf[256];
				sprintf_s(
					buf,
					"[CastleDoorPortal][SKIP_COOLDOWN] castle=%p cooldownFrames=%d\n",
					static_cast< void* >( portal.castle ),
					portal.cooldownFrames
				);
				OutputDebugStringA(buf);
			}
			continue;
		}

		if ( !portal.collider )
			continue;

		for ( const CastleDoorPortalPair& pair : portal.pairs )
		{
			const bool hitSource =
				DoesDoorGroupIntersect(
					portal,
					pair.sourceRefs,
					pair.sourceDoorIndex
				);

			if ( shouldLog && ( hitSource || kEnableTowerDoorPortalVerboseLog ) )
			{
				char buf[512];
				sprintf_s(
					buf,
					"[CastleDoorPortal][PROBE_RESULT] castle=%p %s -> %s sourceRefs=%zu targetRefs=%zu hitSource=%d\n",
					static_cast< void* >( portal.castle ),
					GetCastleDoorFrameDebugName(pair.sourceDoorIndex),
					GetCastleDoorFrameDebugName(pair.targetDoorIndex),
					pair.sourceRefs.size(),
					pair.targetRefs.size(),
					hitSource ? 1 : 0
				);
				OutputDebugStringA(buf);
			}

			if ( !hitSource )
				continue;

			if ( TeleportCastleDoorPair(portal, pair) )
				return true;
		}
	}

	if ( shouldLog )
		DebugPrintTowerDoorPortalLine("[CastleDoorPortal][NO_DOOR_HIT] no source castle door OOBB intersected player capsule");

	return false;
}
#endif

void CGameScene::ResetDynamicGridCounts()
{
	m_sceneGrid.ResetDynamicCounts();
}

bool CGameScene::TryGetTrackedCell(const CGameObject* obj, int& outCellX, int& outCellZ) const
{
	if ( !obj )
		return false;

	const XMFLOAT3 pos = obj->GetPosition();

	if ( pos.y < -100.0f )
		return false;

	return m_sceneGrid.WorldToCell(pos.x, pos.z, outCellX, outCellZ);
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
	m_arrowGridTrackers.clear();
	m_bulletGridTrackers.clear();
}

void CGameScene::RebuildDynamicGridState()
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	ResetDynamicGridCounts();
	BuildDynamicGridTrackers();

	for ( auto& tracker : m_playerGridTrackers )
		RefreshDynamicTracker(tracker, EGridDynamicKind::Player);

	UpdateMegaGridState();
	UpdateCastleCenterMegaGridState();
}

void CGameScene::UpdateDynamicGridState()
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	for ( auto& tracker : m_playerGridTrackers )
		RefreshDynamicTracker(tracker, EGridDynamicKind::Player);

	UpdateMegaGridState();
	UpdateCastleCenterMegaGridState();
}

void CGameScene::UpdateMegaGridState()
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	for ( const GridDynamicTracker& tracker : m_playerGridTrackers )
	{
		if ( !tracker.occupied )
			continue;

		int megaX = -1;
		int megaZ = -1;

		if ( !m_sceneGrid.FineCellToMegaGridCell(tracker.prevCellX, tracker.prevCellZ, megaX, megaZ) )
			continue;

		// 중앙 메가그리드 Castle은 기존 200x200 approach zone으로 진입 처리하지 않는다.
		// Castle 포탈 성공 여부로만 중앙 메가그리드 접근을 처리한다.
		if ( megaX == kCastleCenterMegaGridX &&
			 megaZ == kCastleCenterMegaGridZ )
		{
			if ( m_bLocalPlayerInsideCastleCenterMegaGrid )
			{
				m_sceneGrid.SetMegaGridPlayerApproached(
					kCastleCenterMegaGridX,
					kCastleCenterMegaGridZ,
					true
				);
			}

			continue;
		}

		if ( !m_sceneGrid.IsFineCellInsideMegaGridApproachZone(
			megaX,
			megaZ,
			tracker.prevCellX,
			tracker.prevCellZ) )
		{
			continue;
		}

		if ( !m_sceneGrid.HasMegaGridPlayerApproached(megaX, megaZ) )
		{
			m_sceneGrid.SetMegaGridPlayerApproached(megaX, megaZ, true);
		}
	}
}

XMFLOAT3 CGameScene::ComputeLocalStageTeleportPosition(int megaGridNumber) const
{
	// 입력 번호:
	// 789
	// 456
	// 123
	//
	// 현재 CSceneGrid도 megaNumber = megaZ * 3 + megaX + 1 구조다.
	// 2번 메가그리드 중심이 (0, 0, 0)이 되도록 world grid가 잡혀 있다.
	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return XMFLOAT3(0.0f, 0.0f, 0.0f);

	const int zeroBased = megaGridNumber - 1;
	const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
	const int megaZ = zeroBased / CSceneGrid::kMegaGridRows;

	const float centerX =
		static_cast< float >(
			CSceneGrid::kGridMinX +
			megaX * CSceneGrid::kMegaGridCellWidth +
			CSceneGrid::kMegaGridCellWidth / 2
		);

	const float centerZ =
		static_cast< float >(
			CSceneGrid::kGridMinZ +
			megaZ * CSceneGrid::kMegaGridCellHeight +
			CSceneGrid::kMegaGridCellHeight / 2
		);

	XMFLOAT3 dst{};
	dst.x = centerX;
	dst.y = 0.0f;
	dst.z = centerZ + ( megaGridNumber == 5 ? -200.0f : -150.0f );

	return dst;
}

bool CGameScene::TryTeleportLocalPlayerToMegaGridByNumber(int megaGridNumber)
{
#ifndef USING_NETWORK
	if ( !m_bSimulateLocalStageTeleport )
		return false;

	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return false;

	if ( m_bLocalPlayerDead )
		return false;

	CGameObject* player = GetPlayer();
	if ( !player )
		player = GetPlayerBySlot(0);

	if ( !player )
		return false;

	const XMFLOAT3 dst = ComputeLocalStageTeleportPosition(megaGridNumber);

	player->SetPosition(dst);

	if ( auto* collider = player->GetComponent<CColliderComponent>() )
	{
		collider->UpdateWorldBounds();
	}

	if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
	{
		controller->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
		controller->SetInputDirection(static_cast< DWORD >( 0 ));
		controller->SetRunRequested(false);
	}

	if ( auto* camera = GetMainCamera() )
	{
		XMFLOAT3 cameraTarget = dst;
		cameraTarget.y += 1.7f;

		camera->Update(cameraTarget, 0.0f);
		camera->SetLookAt(cameraTarget);
		camera->RegenerateViewMatrix();
	}

	UpdateDynamicGridState();

	return true;
#else
	UNREFERENCED_PARAMETER(megaGridNumber);
	return false;
#endif
}

void CGameScene::RegisterMonsterToMegaGrid(
	CGameObject* monster,
	const XMFLOAT3& spawnPosition,
	UINT skinnedBatchObjectIndex)
{
	if ( !monster )
		return;

	if ( skinnedBatchObjectIndex >= m_skinnedMonsterMegaGridNumbers.size() )
		m_skinnedMonsterMegaGridNumbers.resize(( size_t ) skinnedBatchObjectIndex + 1, -1);

	const int megaNumber =
		m_sceneGrid.MegaGridNumberFromWorldPosition(spawnPosition.x, spawnPosition.z);

	m_skinnedMonsterMegaGridNumbers[( size_t ) skinnedBatchObjectIndex] = megaNumber;

	if ( megaNumber <= 0 )
		return;

	const uint16_t monsterMegaGridMask =
		static_cast< uint16_t >( 1u << ( megaNumber - 1 ) );

	SetObjectCollisionMegaGridMask(monster, monsterMegaGridMask, true);

	const int zeroBased = megaNumber - 1;
	const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
	const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

	m_sceneGrid.AddMonsterToMegaGrid(megaX, megaZ, monster);
}

int CGameScene::GetLocalPlayerMegaGridNumberForMonsterTick() const
{
	CGameObject* player = GetPlayer();

	if ( !player )
		player = GetPlayerBySlot(0);

	if ( !player )
		return -1;

	const XMFLOAT3 pos = player->GetPosition();
	return m_sceneGrid.MegaGridNumberFromWorldPosition(pos.x, pos.z);
}

bool CGameScene::ShouldSkipMonsterByMegaGrid(
	const CGameObject* monster,
	UINT skinnedBatchObjectIndex,
	int activeMegaGridNumber) const
{
	if ( !monster )
		return false;

	const SkinnedComponentCache* cache =
		GetSkinnedComponentCache(skinnedBatchObjectIndex);

	if ( !cache || cache->object != monster )
		return false;

	if ( !cache->isNpc )
		return false;

	if ( activeMegaGridNumber <= 0 )
		return true;

	if ( skinnedBatchObjectIndex >=
		 static_cast< UINT >( m_skinnedMonsterMegaGridNumbers.size() ) )
	{
		return false;
	}

	const int monsterMegaGridNumber =
		m_skinnedMonsterMegaGridNumbers[
			static_cast< size_t >( skinnedBatchObjectIndex )
		];

	if ( monsterMegaGridNumber <= 0 )
		return false;

	return monsterMegaGridNumber != activeMegaGridNumber;
}

void CGameScene::ResetMonsterToHomeForMegaGridSkip(CGameObject* monster) const
{
	if ( !monster )
		return;

	if ( IsMonsterDead(monster) )
		return;

	auto ResetAI =
		[ ] (CMonsterAIComponent* ai) -> bool
		{
			if ( !ai )
				return false;

			ai->ResetToHomeTransformForMegaGridSkip();
			return true;
		};

	if ( ResetAI(monster->GetComponent<CGhoulAIComponent>()) )
		return;

	if ( ResetAI(monster->GetComponent<CSwordManAIComponent>()) )
		return;

	if ( ResetAI(monster->GetComponent<CBowManAIComponent>()) )
		return;

	if ( ResetAI(monster->GetComponent<CMutantAIComponent>()) )
		return;

	if ( ResetAI(monster->GetComponent<CBossAIComponent>()) )
		return;

	if ( ResetAI(monster->GetComponent<CMonsterAIComponent>()) )
		return;
}

void CGameScene::SetLocalMonsterChaseEnabled(bool enabled)
{
	if ( m_bSimulateLocalMonsterChase == enabled )
		return;

	m_bSimulateLocalMonsterChase = enabled;

	if ( !enabled )
	{
		OutputDebugStringA("[MonsterAI] Local monster chase disabled\n");
		StopAllLocalMonsterChaseAndReturnHome();
	}
	else
	{
		OutputDebugStringA("[MonsterAI] Local monster chase enabled\n");
	}
}

void CGameScene::StopAllLocalMonsterChaseAndReturnHome()
{
	for ( const SkinnedComponentCache& cache : m_skinnedComponentCache )
	{
		if ( !cache.object )
			continue;

		if ( !cache.isNpc )
			continue;

		StopMonsterChaseAndReturnHome(cache.object);
	}
}

void CGameScene::StopMonsterChaseAndReturnHome(CGameObject* monster) const
{
	if ( !monster )
		return;

	if ( IsMonsterDead(monster) )
		return;

	auto StopAI =
		[ ] (CMonsterAIComponent* ai) -> bool
		{
			if ( !ai )
				return false;

			ai->StopChaseAndReturnHome();
			return true;
		};

	if ( StopAI(monster->GetComponent<CGhoulAIComponent>()) )
		return;

	if ( StopAI(monster->GetComponent<CSwordManAIComponent>()) )
		return;

	if ( StopAI(monster->GetComponent<CBowManAIComponent>()) )
		return;

	if ( StopAI(monster->GetComponent<CMutantAIComponent>()) )
		return;

	if ( StopAI(monster->GetComponent<CBossAIComponent>()) )
		return;

	if ( StopAI(monster->GetComponent<CMonsterAIComponent>()) )
		return;
}

uint16_t CGameScene::ComputeStaticObjectMegaGridMask(CGameObject* obj) const
{
	if ( !obj )
		return 0;

	auto* collider = obj->GetComponent<CColliderComponent>();
	if ( !collider )
		return 0;

	uint16_t mask = 0;

	if ( collider->GetType() == EColliderType::OOBB )
	{
		const std::vector<MeshOOBBSet>& meshSets = collider->GetMeshOOBBSets();

		for ( const MeshOOBBSet& set : meshSets )
		{
			for ( const BoundingOrientedBox& box : set.WorldSubOOBBs )
			{
				XMFLOAT3 corners[BoundingOrientedBox::CORNER_COUNT] = {};
				box.GetCorners(corners);

				float minX = corners[0].x;
				float maxX = corners[0].x;
				float minZ = corners[0].z;
				float maxZ = corners[0].z;

				for ( int i = 1; i < BoundingOrientedBox::CORNER_COUNT; ++i )
				{
					minX = min(minX, corners[i].x);
					maxX = max(maxX, corners[i].x);
					minZ = min(minZ, corners[i].z);
					maxZ = max(maxZ, corners[i].z);
				}

				int beginCellX = static_cast< int >(floor(minX)) - CSceneGrid::kGridMinX;
				int endCellX = static_cast< int >(ceil(maxX)) - CSceneGrid::kGridMinX - 1;
				int beginCellZ = static_cast< int >(floor(minZ)) - CSceneGrid::kGridMinZ;
				int endCellZ = static_cast< int >( ceil(maxZ) ) - CSceneGrid::kGridMinZ - 1;

				beginCellX = std::clamp(beginCellX, 0, CSceneGrid::kGridWidth - 1);
				endCellX = std::clamp(endCellX, 0, CSceneGrid::kGridWidth - 1);
				beginCellZ = std::clamp(beginCellZ, 0, CSceneGrid::kGridHeight - 1);
				endCellZ = std::clamp(endCellZ, 0, CSceneGrid::kGridHeight - 1);

				if ( beginCellX > endCellX || beginCellZ > endCellZ )
					continue;

				const int beginMegaX = beginCellX / CSceneGrid::kMegaGridCellWidth;
				const int endMegaX = endCellX / CSceneGrid::kMegaGridCellWidth;
				const int beginMegaZ = beginCellZ / CSceneGrid::kMegaGridCellHeight;
				const int endMegaZ = endCellZ / CSceneGrid::kMegaGridCellHeight;

				for ( int mz = beginMegaZ; mz <= endMegaZ; ++mz )
				{
					for ( int mx = beginMegaX; mx <= endMegaX; ++mx )
					{
						if ( mx < 0 || mx >= CSceneGrid::kMegaGridCols )
							continue;

						if ( mz < 0 || mz >= CSceneGrid::kMegaGridRows )
							continue;

						const int bit = mz * CSceneGrid::kMegaGridCols + mx;
						mask |= static_cast< uint16_t >(1u << bit);
					}
				}
			}
		}
	}

	if ( mask != 0 )
		return mask;

	return ComputeObjectCurrentMegaGridMask(obj);
}

uint16_t CGameScene::ComputeObjectCurrentMegaGridMask(const CGameObject* obj) const
{
	if ( !obj )
		return 0;

	const XMFLOAT3 pos = obj->GetPosition();

	int megaX = -1;
	int megaZ = -1;

	if ( !m_sceneGrid.TryGetMegaGridFromWorldPosition(pos.x, pos.z, megaX, megaZ) )
		return 0;

	const int bit = megaZ * CSceneGrid::kMegaGridCols + megaX;
	return static_cast< uint16_t >( 1u << bit );
}

void CGameScene::SetObjectCollisionMegaGridMask(
	CGameObject* obj,
	uint16_t mask,
	bool fixedMask)
{
	if ( !obj )
		return;

	CColliderComponent* collider = obj->GetComponent<CColliderComponent>();
	if ( !collider )
		return;

	collider->SetCollisionMegaGridMask(mask);
	collider->SetCollisionMegaGridMaskFixed(fixedMask);
}

void CGameScene::RefreshDynamicCollisionMegaGridMasks()
{
	auto RefreshObject = [ this ] (CGameObject* obj)
		{
			if ( !obj )
				return;

			CColliderComponent* collider = obj->GetComponent<CColliderComponent>();
			if ( !collider )
				return;

			if ( collider->IsCollisionMegaGridMaskFixed() )
				return;

			const uint16_t mask = ComputeObjectCurrentMegaGridMask(obj);
			collider->SetCollisionMegaGridMask(mask);
		};

	// 플레이어는 위치가 계속 변한다.
	for ( CGameObject* player : m_playersBySlot )
		RefreshObject(player);

	// static batch에 있지만 transform이 변하는 gameplay object들.
	for ( CGameObject* obj : m_staticGameplayTickObjects )
		RefreshObject(obj);

	// PlayerBow / EnemyBow는 skinned batch 쪽이므로 staticGameplayTickObjects에 없다.
	for ( CGameObject* obj : m_PlayerBowRefs )
		RefreshObject(obj);

	for ( CGameObject* obj : m_EnemyBowRefs )
		RefreshObject(obj);
}

bool CGameScene::ShouldKeepCollisionPairByMegaGrid(
	const CColliderComponent* a,
	const CColliderComponent* b) const
{
	if ( !a || !b )
		return false;

	const uint16_t maskA = a->GetCollisionMegaGridMask();
	const uint16_t maskB = b->GetCollisionMegaGridMask();

	// grid 밖으로 나간 투사체/오브젝트는 충돌 후보에서 제거.
	if ( maskA == 0 || maskB == 0 )
		return false;

	return ( maskA & maskB ) != 0;
}

void CGameScene::MarkLocalPlayerEnteredCastleCenterMegaGrid()
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	m_bLocalPlayerInsideCastleCenterMegaGrid = true;

	// Castle 포탈을 탄 순간 중앙 메가그리드 중앙에 접근한 것으로 처리한다.
	// 기존 200x200 approach zone 판정은 중앙 메가그리드에서는 쓰지 않는다.
	m_sceneGrid.SetMegaGridPlayerApproached(
		kCastleCenterMegaGridX,
		kCastleCenterMegaGridZ,
		true
	);
}

bool CGameScene::IsLocalPlayerInsideCastleCenterMegaGridFullArea() const
{
	if ( !m_sceneGrid.IsInitialized() )
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

	if ( !m_sceneGrid.FineCellToMegaGridCell(
		tracker.prevCellX,
		tracker.prevCellZ,
		megaX,
		megaZ) )
	{
		return false;
	}

	return
		megaX == kCastleCenterMegaGridX &&
		megaZ == kCastleCenterMegaGridZ;
}

void CGameScene::UpdateCastleCenterMegaGridState()
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	if ( !m_bLocalPlayerInsideCastleCenterMegaGrid )
		return;

	// Castle 포탈 후에는 중앙 메가그리드 400x400 안에 있는 동안
	// 계속 중앙 메가그리드 내부로 취급한다.
	if ( IsLocalPlayerInsideCastleCenterMegaGridFullArea() )
	{
		m_sceneGrid.SetMegaGridPlayerApproached(
			kCastleCenterMegaGridX,
			kCastleCenterMegaGridZ,
			true
		);
		return;
	}

	// 죽고 부활해서 중앙 메가그리드 400x400 밖으로 나간 경우.
	m_bLocalPlayerInsideCastleCenterMegaGrid = false;

	m_sceneGrid.SetMegaGridPlayerApproached(
		kCastleCenterMegaGridX,
		kCastleCenterMegaGridZ,
		false
	);
}

void CGameScene::DumpStaticGridOccupancyLog() const
{
	m_sceneGrid.DumpStaticGridOccupancyLog();
}

void CGameScene::SetMegaGridApproachZoneSize(int megaX, int megaZ, int widthCells, int heightCells)
{
	m_sceneGrid.SetMegaGridApproachZoneSize(megaX, megaZ, widthCells, heightCells);
}

void CGameScene::SetMegaGridCleared(int megaX, int megaZ, bool cleared)
{
	m_sceneGrid.SetMegaGridCleared(megaX, megaZ, cleared);
}

void CGameScene::SetMegaGridEventOccurred(int megaX, int megaZ, bool occurred)
{
	m_sceneGrid.SetMegaGridEventOccurred(megaX, megaZ, occurred);
}

bool CGameScene::HasMegaGridPlayerApproached(int megaX, int megaZ) const
{
	return m_sceneGrid.HasMegaGridPlayerApproached(megaX, megaZ);
}

bool CGameScene::IsMegaGridCleared(int megaX, int megaZ) const
{
	return m_sceneGrid.IsMegaGridCleared(megaX, megaZ);
}

bool CGameScene::HasMegaGridEventOccurred(int megaX, int megaZ) const
{
	return m_sceneGrid.HasMegaGridEventOccurred(megaX, megaZ);
}


CGameScene::~CGameScene()
{
}

void CGameScene::ReleaseObjects()
{
    m_staticBatch.shader.reset();
    m_skinnedBatch.shader.reset();

	m_treeStaticShader.reset();
	m_treeAlphaClipObjects.clear();
	m_skinnedAlphaClipObjects.clear();

	m_staticTreeGridCullFlags.clear();
	m_staticDynamicWorldMatrixFlags.clear();
	m_staticShadowCasterFlags.clear();
	m_staticTreeObjectIndices.clear();
	m_staticShadowOcclusionEntryIndices.clear();
	m_staticCollisionMegaGridMasks.clear();
	m_skinnedShadowOcclusionEntryIndices.clear();

#ifndef USING_NETWORK
	m_towerDoorPortals.clear();
	m_castleDoorPortals.clear();
#endif

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

	m_ghoulRefs.clear();
	m_swordManRefs.clear();
	m_bowManRefs.clear();
	m_MutantRefs.clear();
	m_bossRefs.clear();

	m_mutantKeyTriggerMegaByObject.clear();
	m_mutantKeyTriggerRegisteredByMega.fill(false);

	m_helmetRefs.clear();
	m_arrowRefs.clear();
	m_bulletRefs.clear();

	m_attachmentBinds.clear();
	m_staticInstanceGroups.clear();
	ResetStaticWorldLodEntries();
	ResetStaticOcclusionEntries();
	m_staticOcclusionUnitBoxMesh.reset();

	m_skinnedInstanceGroups.clear();
	m_skinnedComponentCache.clear();

	ResetSkinnedWorldLodEntries();
	ResetSkinnedOcclusionEntries();

    m_PlayerSwordRefs.clear();
    m_PlayerBowRefs.clear();
    m_PlayerAxeRefs.clear();
    m_PlayerGunRefs.clear();

    m_EnemySwordRefs.clear();
    m_EnemyBowRefs.clear();

	ResetPlayerFootstepSfxState();
	ResetMonsterSfxState();

	m_preparedPlayerArrows = { nullptr, nullptr, nullptr, nullptr }; 
	m_prevBowLoadPhase = { false, false, false, false };
	m_prevBowReleasePhase = { false, false, false, false };
	m_preparedBowmanArrows.clear();
	m_prevEnemyBowReleasePhase.clear();

	m_hud.ReleaseResources();
	m_depthFog.ReleaseResources();

	m_occlusionStaticShader.reset();
	m_shadowStaticShader.reset();
	m_shadowAlphaClipStaticShader.reset();
	m_shadowSkinnedShader.reset();
	m_shadowAlphaClipSkinnedShader.reset();
	m_skinnedAlphaClipShader.reset();

	m_sceneRenderTargetCount = 0;
	m_bSceneRenderTargetsReady = false;
	m_bInactiveOverlayVisible = false;
	m_bStartedGameplayMusic = false;
	m_bWasLocalPlayerInsideMegaGridCenter = false;
	m_bLocalPlayerInsideCastleCenterMegaGrid = false;

	m_navMesh.reset();

	m_bLocalPlayerDead = false;
	m_bLocalPlayerRespawnUsed = false;
	m_localPlayerRespawnTimer = 0.0f;
#ifdef USING_NETWORK
	m_prevPlayerNetworkStateCode.clear();
#endif
	m_deadMonsters.clear();
	m_skinnedMonsterMegaGridNumbers.clear();

	m_itemBillboardShader.reset();
	m_transparentItemBillboardShader.reset();

	m_itemBillboardQuadMesh.reset();
	m_itemBillboards.clear();
	m_keyItemTexture.reset();

	ReleaseItemBillboardGpuResources();

	m_muzzleFlashShader.reset();
	m_muzzleFlashes.clear();

	m_swordTrailShader.reset();
	m_swordTrails.clear();

	m_monsterSwordTrailShader.reset();
	m_monsterSwordTrails.clear();

	m_staticRenderObjectCache.clear();
	m_staticGameplayTickObjects.clear();

#ifndef USING_NETWORK
	m_monsterSpawnEntries.clear();
#endif

	ShutdownSpatialGrid();

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

	if ( m_itemBillboardQuadMesh )
		m_itemBillboardQuadMesh->ReleaseUploadBuffers();

	if ( m_keyItemTexture )
		m_keyItemTexture->ReleaseUploadBuffers();
}

void CGameScene::ReleaseShaderVariables()
{
	ReleaseItemBillboardGpuResources();
	ReleaseStaticOcclusionGpuResources();
	ReleaseSkinnedOcclusionGpuResources();

	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		if ( m_pd3dStaticInstanceBuffer[frameIndex] )
		{
			if ( m_pMappedStaticInstanceBuffer[frameIndex] )
			{
				m_pd3dStaticInstanceBuffer[frameIndex]->Unmap(0, NULL);
				m_pMappedStaticInstanceBuffer[frameIndex] = nullptr;
			}

			m_pd3dStaticInstanceBuffer[frameIndex].Reset();
		}

		m_pMappedStaticInstanceBuffer[frameIndex] = nullptr;
	}

	m_staticInstanceBufferCapacity = 0;

	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		if ( m_pd3dSkinnedInstanceBuffer[frameIndex] )
		{
			if ( m_pMappedSkinnedInstanceBuffer[frameIndex] )
			{
				m_pd3dSkinnedInstanceBuffer[frameIndex]->Unmap(0, NULL);
				m_pMappedSkinnedInstanceBuffer[frameIndex] = nullptr;
			}

			m_pd3dSkinnedInstanceBuffer[frameIndex].Reset();
		}

		m_pMappedSkinnedInstanceBuffer[frameIndex] = nullptr;
	}

	m_skinnedInstanceBufferCapacity = 0;

	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		if ( m_pd3dSkinnedBonePaletteBuffer[frameIndex] )
		{
			if ( m_pMappedSkinnedBonePaletteBuffer[frameIndex] )
			{
				m_pd3dSkinnedBonePaletteBuffer[frameIndex]->Unmap(0, NULL);
				m_pMappedSkinnedBonePaletteBuffer[frameIndex] = nullptr;
			}

			m_pd3dSkinnedBonePaletteBuffer[frameIndex].Reset();
		}

		m_pMappedSkinnedBonePaletteBuffer[frameIndex] = nullptr;
	}

	m_skinnedBonePaletteBaseByObject.clear();
	m_skinnedBonePaletteCountByObject.clear();
	m_skinnedBonePaletteCapacity = 0;

	// ---- Static batch CB ----
	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		if ( m_staticBatch.cbGameObjects[frameIndex] )
		{
			if ( m_staticBatch.mappedGameObjects[frameIndex] )
			{
				m_staticBatch.cbGameObjects[frameIndex]->Unmap(0, NULL);
				m_staticBatch.mappedGameObjects[frameIndex] = nullptr;
			}

			m_staticBatch.cbGameObjects[frameIndex].Reset();
		}

		m_staticBatch.mappedGameObjects[frameIndex] = nullptr;
		m_staticBatch.baseCbvGpu[frameIndex] = D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
	}

	// ---- Skinned batch CB ----
	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		if ( m_skinnedBatch.cbGameObjects[frameIndex] )
		{
			if ( m_skinnedBatch.mappedGameObjects[frameIndex] )
			{
				m_skinnedBatch.cbGameObjects[frameIndex]->Unmap(0, NULL);
				m_skinnedBatch.mappedGameObjects[frameIndex] = nullptr;
			}

			m_skinnedBatch.cbGameObjects[frameIndex].Reset();
		}

		m_skinnedBatch.mappedGameObjects[frameIndex] = nullptr;
		m_skinnedBatch.baseCbvGpu[frameIndex] = D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
	}

	for ( UINT i = 0; i < kFrameResourceCount; ++i )
	{
		if ( m_pd3dcbLights[i] )
		{
			m_pd3dcbLights[i]->Unmap(0, NULL);
			m_pd3dcbLights[i].Reset();
		}

		m_pcbMappedLights[i] = nullptr;
	}

	m_nLightsCBElementBytes = 0;

	for ( UINT i = 0; i < kFrameResourceCount; ++i )
	{
		if ( m_pd3dcbMaterials[i] )
		{
			m_pd3dcbMaterials[i]->Unmap(0, NULL);
			m_pd3dcbMaterials[i].Reset();
		}

		m_pcbMappedMaterials[i] = nullptr;
	}

	m_nMaterialsCBElementBytes = 0;
	m_nFrameResourceIndex = 0;

	m_depthFog.ReleaseConstantBuffer();

	m_shadowMap.ReleaseResources();

	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		if ( m_colliderBatch.cbGameObjects[frameIndex] )
		{
			if ( m_colliderBatch.mappedGameObjects[frameIndex] )
			{
				m_colliderBatch.cbGameObjects[frameIndex]->Unmap(0, NULL);
				m_colliderBatch.mappedGameObjects[frameIndex] = nullptr;
			}

			m_colliderBatch.cbGameObjects[frameIndex].Reset();
		}

		m_colliderBatch.mappedGameObjects[frameIndex] = nullptr;
		m_colliderBatch.baseCbvGpu[frameIndex] = D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
	}

	m_hud.ReleaseResources();
	m_depthFog.ReleaseShaderVariables();
}

float CGameScene::QuaternionToYawDegrees(const XMFLOAT4& q)
{
    // yaw(heading) only
    const float siny_cosp = 2.0f * (q.w * q.y + q.x * q.z);
    const float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);

    const float yawRad = std::atan2(siny_cosp, cosy_cosp);
    return XMConvertToDegrees(yawRad);
}

bool CGameScene::IsStaticTreeObject(const CGameObject* obj) const
{
	if ( !obj )
		return false;

	return m_treeAlphaClipObjects.find(obj) != m_treeAlphaClipObjects.end();
}

void CGameScene::UpdateStaticTreeGridCullSelection(CCamera* camera)
{
	const size_t objectCount = m_staticBatch.objectRefs.size();

	if ( m_staticTreeGridCullFlags.size() != objectCount )
		m_staticTreeGridCullFlags.assign(objectCount, 0);
	else
		std::fill(m_staticTreeGridCullFlags.begin(), m_staticTreeGridCullFlags.end(), 0);

	if ( !m_bStaticTreeGridCullingEnabled )
		return;

	if ( !camera )
		return;

	const bool shouldCullTrees = ShouldCullTreesByVillageGrid(camera);

	if ( !shouldCullTrees )
		return;

	for ( UINT objectIndex : m_staticTreeObjectIndices )
	{
		if ( objectIndex >= ( UINT ) m_staticTreeGridCullFlags.size() )
			continue;

		m_staticTreeGridCullFlags[objectIndex] = 1;
	}
}

void CGameScene::BuildStaticInstanceGroups()
{
	PROFILE_RENDER_SCOPE("BuildStaticInstanceGroups::Total");

	m_staticInstanceGroups.clear();
	BuildStaticWorldLodEntryIndexMap();

	std::unordered_map<StaticGroupKey, size_t, StaticGroupKeyHash> groupIndexByKey;
	groupIndexByKey.reserve(m_staticBatch.objectRefs.size() * 4);

	m_staticInstanceGroups.reserve(m_staticBatch.objectRefs.size() * 2);

	auto AddObjectToGroup =
		[ & ](
			UINT objectIndex,
			const std::shared_ptr<CMesh>& mesh,
			int lodLevel,
			bool useTreeShader)
		{
			if ( !mesh )
				return;

			const UINT subMeshCount =
				static_cast< UINT >( mesh->m_SubMeshes.size() );

			for ( UINT subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex )
			{
				StaticGroupKey key{};
				key.mesh = mesh.get();
				key.subMeshIndex = subMeshIndex;
				key.useTreeShader = useTreeShader;
				key.lodLevel = lodLevel;

				size_t groupIndex = 0;

				auto it = groupIndexByKey.find(key);

				if ( it == groupIndexByKey.end() )
				{
					StaticInstanceGroup group{};
					group.mesh = mesh;
					group.subMeshIndex = subMeshIndex;
					group.useTreeShader = useTreeShader;
					group.lodLevel = lodLevel;

					groupIndex = m_staticInstanceGroups.size();
					m_staticInstanceGroups.push_back(std::move(group));

					groupIndexByKey.emplace(key, groupIndex);
				}
				else
				{
					groupIndex = it->second;
				}

				m_staticInstanceGroups[groupIndex].objectIndices.push_back(objectIndex);
			}
		};

	for ( UINT objectIndex = 0;
		  objectIndex < static_cast< UINT >(m_staticBatch.objectRefs.size());
		  ++objectIndex )
	{
		CGameObject* obj = m_staticBatch.objectRefs[objectIndex];
		if ( !obj )
			continue;

		const bool useTreeShader =
			( m_treeAlphaClipObjects.find(obj) != m_treeAlphaClipObjects.end() );

		int lodEntryIndex = -1;
		if ( objectIndex <
			 static_cast< UINT >(m_staticWorldLodEntryIndexByObjectIndex.size()) )
		{
			lodEntryIndex = m_staticWorldLodEntryIndexByObjectIndex[objectIndex];
		}

		const bool hasLodEntry =
			( lodEntryIndex >= 0 &&
			  lodEntryIndex < static_cast< int >(m_staticWorldLodEntries.size()) );

		if ( hasLodEntry )
		{
			const StaticWorldLodEntry& entry =
				m_staticWorldLodEntries[lodEntryIndex];

			if ( entry.lodEnabled )
			{
				bool registeredResolvedLods[3] = { false, false, false };

				for ( int lodLevel = 0; lodLevel < 3; ++lodLevel )
				{
					const int resolvedLod =
						ResolveStaticWorldLodLevel(entry, lodLevel);

					if ( resolvedLod < 0 || resolvedLod > 2 )
						continue;

					if ( registeredResolvedLods[resolvedLod] )
						continue;

					registeredResolvedLods[resolvedLod] = true;

					std::shared_ptr<CMesh> lodMesh =
						entry.lodMeshes[static_cast< size_t >( resolvedLod )];

					AddObjectToGroup(
						objectIndex,
						lodMesh,
						resolvedLod,
						useTreeShader
					);
				}

				continue;
			}
		}

		// LOD 대상이 아닌 일반 static object는 기존처럼 현재 mesh만 등록.
		const int meshCount = obj->GetMeshCount();

		for ( int meshIndex = 0; meshIndex < meshCount; ++meshIndex )
		{
			std::shared_ptr<CMesh> mesh = obj->GetMeshShared(meshIndex);
			AddObjectToGroup(objectIndex, mesh, 0, useTreeShader);
		}
	}

	std::sort(
		m_staticInstanceGroups.begin(),
		m_staticInstanceGroups.end(),
		[ ] (const StaticInstanceGroup& a, const StaticInstanceGroup& b)
		{
			if ( a.useTreeShader != b.useTreeShader )
				return a.useTreeShader < b.useTreeShader;

			if ( a.mesh.get() != b.mesh.get() )
				return a.mesh.get() < b.mesh.get();

			if ( a.subMeshIndex != b.subMeshIndex )
				return a.subMeshIndex < b.subMeshIndex;

			return a.lodLevel < b.lodLevel;
		}
	);

	UINT runningStart = 0;

	for ( StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		group.instanceBufferStart = runningStart;
		runningStart += static_cast< UINT >(group.objectIndices.size());
	}

	m_staticInstanceBufferCapacity = runningStart;

	for ( StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		group.visibleSceneObjectIndices.clear();
		group.visibleShadowObjectIndices.clear();

		group.visibleSceneObjectIndices.reserve(group.objectIndices.size());
		group.visibleShadowObjectIndices.reserve(group.objectIndices.size());
	}
}

void CGameScene::BuildSkinnedComponentCache()
{
	m_skinnedComponentCache.clear();

	const UINT objectCount =
		static_cast< UINT >( m_skinnedBatch.objectRefs.size() );

	m_skinnedComponentCache.resize(objectCount);

	for ( UINT objectIndex = 0; objectIndex < objectCount; ++objectIndex )
	{
		CGameObject* obj = m_skinnedBatch.objectRefs[objectIndex];

		SkinnedComponentCache& cache = m_skinnedComponentCache[objectIndex];
		cache = SkinnedComponentCache{};
		cache.object = obj;

		if ( !obj )
			continue;

		cache.renderer = obj->GetComponent<CSkinnedMeshRendererComponent>();
		cache.skinning = obj->GetComponent<CSkinningComponent>();
		cache.animator = obj->GetComponent<CAnimatorComponent>();
		cache.health = obj->GetComponent<CHealthComponent>();
		cache.actorTag = obj->GetComponent<CActorTagComponent>();
		cache.collider = obj->GetComponent<CColliderComponent>();

		if ( cache.actorTag )
		{
			cache.isNpc = ( cache.actorTag->kind == EActorKind::NPC );
			cache.isPlayer = ( cache.actorTag->kind == EActorKind::Player );
		}
	}
}

const SkinnedComponentCache* CGameScene::GetSkinnedComponentCache(UINT objectIndex) const
{
	if ( objectIndex >= static_cast< UINT >( m_skinnedComponentCache.size() ) )
		return nullptr;

	return &m_skinnedComponentCache[objectIndex];
}

bool CGameScene::WriteSkinnedInstanceVertexFromCache(
	SkinnedInstanceVertex& dst,
	const SkinnedComponentCache& cache,
	UINT objectIndex,
	UINT meshIndex,
	UINT subMeshIndex,
	XMFLOAT4X4* mappedSkinnedBonePaletteBuffer) const
{
	CGameObject* obj = cache.object;
	if ( !obj )
		return false;

	if ( !cache.renderer || !cache.renderer->IsEnabled() )
		return false;

	CSkinningComponent* skin = cache.skinning;
	if ( !skin || !skin->IsSkinned() )
		return false;

	std::shared_ptr<CMesh> objMesh = obj->GetMeshShared(static_cast< int >( meshIndex ));
	if ( !objMesh )
		return false;

	if ( subMeshIndex >= objMesh->m_SubMeshes.size() )
		return false;

	if ( objectIndex >= static_cast< UINT >( m_skinnedBonePaletteBaseByObject.size() ) )
		return false;

	if ( objectIndex >= static_cast< UINT >( m_skinnedBonePaletteCountByObject.size() ) )
		return false;

	const UINT bonePaletteBase = m_skinnedBonePaletteBaseByObject[objectIndex];

	const UINT reservedBoneCount =
		m_skinnedBonePaletteCountByObject[objectIndex];

	if ( reservedBoneCount == 0 )
		return false;

	if ( bonePaletteBase >= m_skinnedBonePaletteCapacity )
		return false;

	if ( reservedBoneCount > m_skinnedBonePaletteCapacity - bonePaletteBase )
		return false;

	const SubMesh& objSm = objMesh->m_SubMeshes[subMeshIndex];

	const XMFLOAT4X4& W = obj->GetWorldMatrix();

	dst.world0 = XMFLOAT4(W._11, W._12, W._13, W._14);
	dst.world1 = XMFLOAT4(W._21, W._22, W._23, W._24);
	dst.world2 = XMFLOAT4(W._31, W._32, W._33, W._34);
	dst.world3 = XMFLOAT4(W._41, W._42, W._43, W._44);

	dst.materialId = ( objSm.materialId == 0xFFFFFFFFu ) ? 0u : objSm.materialId;
	dst.bonePaletteBase = bonePaletteBase;

	const XMFLOAT4X4* srcBoneMats = skin->GetMappedBoneMatrices();
	const UINT boneCount = static_cast< UINT >( skin->GetBoneCount() );

	if ( mappedSkinnedBonePaletteBuffer && srcBoneMats && boneCount > 0 )
	{
		UINT copyBoneCount = boneCount;

		if ( copyBoneCount > reservedBoneCount )
			copyBoneCount = reservedBoneCount;

		memcpy(
			mappedSkinnedBonePaletteBuffer + bonePaletteBase,
			srcBoneMats,
			sizeof(XMFLOAT4X4) * copyBoneCount
		);
	}

	return true;
}

void CGameScene::BuildSkinnedInstanceGroups()
{
	m_skinnedInstanceGroups.clear();
	m_skinnedInstanceGroups.reserve(m_skinnedBatch.objectRefs.size() * 2);

	std::unordered_map<SkinnedGroupKey, size_t, SkinnedGroupKeyHash> groupIndexByKey;
	groupIndexByKey.reserve(m_skinnedBatch.objectRefs.size() * 2);

	for ( UINT objectIndex = 0;
		  objectIndex < static_cast< UINT >(m_skinnedBatch.objectRefs.size());
		  ++objectIndex )
	{
		CGameObject* obj = m_skinnedBatch.objectRefs[objectIndex];
		if ( !obj )
			continue;

		const bool useAlphaClipShader =
			( m_skinnedAlphaClipObjects.find(obj) != m_skinnedAlphaClipObjects.end() );

		const int meshCount = obj->GetMeshCount();

		for ( int meshIndex = 0; meshIndex < meshCount; ++meshIndex )
		{
			std::shared_ptr<CMesh> mesh = obj->GetMeshShared(meshIndex);
			if ( !mesh )
				continue;

			std::string geometryKey = mesh->GetSourceMeshPath();

			if ( geometryKey.empty() )
			{
				char buf[64];
				sprintf_s(buf, "meshptr_%p", mesh.get());
				geometryKey = buf;
			}

			const UINT subMeshCount =
				static_cast< UINT >( mesh->m_SubMeshes.size() );

			for ( UINT subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex )
			{
				SkinnedGroupKey key{};
				key.geometryKey = geometryKey;
				key.meshIndex = static_cast< UINT >(meshIndex);
				key.subMeshIndex = subMeshIndex;
				key.useAlphaClipShader = useAlphaClipShader;

				size_t groupIndex = 0;

				auto it = groupIndexByKey.find(key);

				if ( it == groupIndexByKey.end() )
				{
					SkinnedInstanceGroup newGroup{};
					newGroup.geometryKey = geometryKey;
					newGroup.mesh = mesh;
					newGroup.meshIndex = static_cast< UINT >( meshIndex );
					newGroup.subMeshIndex = subMeshIndex;
					newGroup.useAlphaClipShader = useAlphaClipShader;

					groupIndex = m_skinnedInstanceGroups.size();
					m_skinnedInstanceGroups.push_back(std::move(newGroup));

					groupIndexByKey.emplace(std::move(key), groupIndex);
				}
				else
				{
					groupIndex = it->second;
				}

				m_skinnedInstanceGroups[groupIndex].objectIndices.push_back(objectIndex);
			}
		}
	}

	std::sort(
		m_skinnedInstanceGroups.begin(),
		m_skinnedInstanceGroups.end(),
		[ ] (const SkinnedInstanceGroup& a, const SkinnedInstanceGroup& b)
		{
			if ( a.useAlphaClipShader != b.useAlphaClipShader )
				return a.useAlphaClipShader < b.useAlphaClipShader; // opaque 먼저, alpha-clip 나중

			if ( a.geometryKey != b.geometryKey )
				return a.geometryKey < b.geometryKey;

			if ( a.meshIndex != b.meshIndex )
				return a.meshIndex < b.meshIndex;

			return a.subMeshIndex < b.subMeshIndex;
		}
	);

	UINT runningStart = 0;

	for ( SkinnedInstanceGroup& group : m_skinnedInstanceGroups )
	{
		group.instanceBufferStart = runningStart;
		runningStart += static_cast< UINT >(group.objectIndices.size());
	}

	m_skinnedInstanceBufferCapacity = runningStart;
}

void CGameScene::BuildStaticGameplayTickList()
{
	m_staticGameplayTickObjects.clear();

	const size_t total =
		m_helmetRefs.size() +
		m_PlayerSwordRefs.size() +
		m_PlayerAxeRefs.size() +
		m_PlayerGunRefs.size() +
		m_EnemySwordRefs.size() +
		m_arrowRefs.size() +
		m_bulletRefs.size();

	m_staticGameplayTickObjects.reserve(total);

	auto AppendRefs = [ this ] (const std::vector<CGameObject*>& refs)
		{
			for ( CGameObject* obj : refs )
			{
				if ( obj )
					m_staticGameplayTickObjects.push_back(obj);
			}
		};

	AppendRefs(m_helmetRefs);
	AppendRefs(m_PlayerSwordRefs);
	AppendRefs(m_PlayerAxeRefs);
	AppendRefs(m_PlayerGunRefs);
	AppendRefs(m_EnemySwordRefs);
	AppendRefs(m_arrowRefs);
	AppendRefs(m_bulletRefs);

	char buf[256];
	sprintf_s(
		buf,
		"[StaticGameplayTickList] tickObjects=%zu / staticObjects=%zu\n",
		m_staticGameplayTickObjects.size(),
		m_staticBatch.objectRefs.size()
	);
	OutputDebugStringA(buf);
}

void CGameScene::BuildStaticRenderObjectCache()
{
	m_staticRenderObjectCache.clear();
	m_staticRenderObjectCache.resize(m_staticBatch.objectRefs.size());

	for ( UINT i = 0; i < static_cast< UINT >(m_staticBatch.objectRefs.size()); ++i )
	{
		CGameObject* obj = m_staticBatch.objectRefs[i];

		StaticRenderObjectCache& cache = m_staticRenderObjectCache[i];
		cache.object = obj;

		if ( !obj )
			continue;

		cache.renderer = obj->GetComponent<CStaticMeshRendererComponent>();

		cache.dynamicWorldMatrix =
			( i < static_cast< UINT >(m_staticDynamicWorldMatrixFlags.size()) ) &&
			( m_staticDynamicWorldMatrixFlags[i] != 0 );

		const XMFLOAT4X4& W = obj->GetWorldMatrix();

		StoreStaticWorldRows(
			cache.world0,
			cache.world1,
			cache.world2,
			cache.world3,
			W
		);
	}
}

bool CGameScene::WriteStaticInstanceVertexFromCache(
	StaticInstanceVertex& dst,
	UINT objectIndex) const
{
	if ( objectIndex >= static_cast< UINT >( m_staticRenderObjectCache.size() ) )
		return false;

	const StaticRenderObjectCache& cache = m_staticRenderObjectCache[objectIndex];

	if ( !cache.object )
		return false;

	if ( !cache.renderer )
		return false;

	if ( !cache.renderer->IsEnabled() )
		return false;

	if ( cache.dynamicWorldMatrix )
	{
		const XMFLOAT4X4& W = cache.object->GetWorldMatrix();

		StoreStaticWorldRows(
			dst.world0,
			dst.world1,
			dst.world2,
			dst.world3,
			W
		);
	}
	else
	{
		dst.world0 = cache.world0;
		dst.world1 = cache.world1;
		dst.world2 = cache.world2;
		dst.world3 = cache.world3;
	}

	dst.objectId = objectIndex;

	return true;
}

void CGameScene::BuildStaticVisibleListsForFrame(CCamera* camera)
{
	for ( StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		group.visibleSceneObjectIndices.clear();

		for ( UINT objectIndex : group.objectIndices )
		{
			if ( objectIndex >= static_cast< UINT >( m_staticBatch.objectRefs.size() ) )
				continue;

			if ( objectIndex >= static_cast< UINT >( m_staticRenderObjectCache.size() ) )
				continue;

			if ( group.lodLevel != GetStaticObjectActiveLodLevel(objectIndex) )
				continue;

			const StaticRenderObjectCache& cache =
				m_staticRenderObjectCache[objectIndex];

			if ( !cache.object )
				continue;

			if ( !cache.renderer )
				continue;

			if ( !cache.renderer->IsEnabled() )
				continue;

			if ( objectIndex < static_cast< UINT >(m_staticDistanceCullFlags.size()) &&
				 m_staticDistanceCullFlags[objectIndex] != 0 )
			{
				continue;
			}

			if ( objectIndex < static_cast< UINT >(m_staticTreeGridCullFlags.size()) &&
				 m_staticTreeGridCullFlags[objectIndex] != 0 )
			{
				continue;
			}

			const bool cameraVisible =
				( camera == nullptr ) || cache.object->IsVisible(camera);

			if ( !cameraVisible )
				continue;

			if ( objectIndex < static_cast< UINT >(m_staticOcclusionCullFlags.size()) &&
				 m_staticOcclusionCullFlags[objectIndex] != 0 )
			{
				continue;
			}

			group.visibleSceneObjectIndices.push_back(objectIndex);
		}
	}
}

void CGameScene::BuildStaticShadowVisibleListsForFrame()
{

	for ( StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		group.visibleShadowObjectIndices.clear();

		for ( UINT objectIndex : group.objectIndices )
		{
			if ( objectIndex >= static_cast< UINT >( m_staticBatch.objectRefs.size() ) )
				continue;

			if ( objectIndex >= static_cast< UINT >( m_staticRenderObjectCache.size() ) )
				continue;

			if ( group.lodLevel != GetStaticObjectActiveLodLevel(objectIndex) )
				continue;

			const StaticRenderObjectCache& cache =
				m_staticRenderObjectCache[objectIndex];

			if ( !cache.object )
				continue;

			if ( !cache.renderer )
				continue;

			if ( !cache.renderer->IsEnabled() )
				continue;

			if ( objectIndex < static_cast< UINT >(m_staticShadowCasterFlags.size()) &&
				 m_staticShadowCasterFlags[objectIndex] == 0 )
			{
				continue;
			}

			if ( objectIndex < static_cast< UINT >(m_staticDistanceCullFlags.size()) &&
				 m_staticDistanceCullFlags[objectIndex] != 0 )
			{
				continue;
			}

			if ( objectIndex < static_cast< UINT >(m_staticTreeGridCullFlags.size()) &&
				 m_staticTreeGridCullFlags[objectIndex] != 0 )
			{
				continue;
			}

			if ( !IsStaticObjectInsideShadowBox(objectIndex) )
				continue;

			group.visibleShadowObjectIndices.push_back(objectIndex);
		}
	}
}

void CGameScene::RenderStaticInstanceGroups(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::RenderStaticInstanceGroups");

	if ( !cmd ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	ID3D12Resource* staticInstanceBuffer =
		m_pd3dStaticInstanceBuffer[frameIndex].Get();

	StaticInstanceVertex* mappedStaticInstanceBuffer =
		m_pMappedStaticInstanceBuffer[frameIndex];

	if ( !staticInstanceBuffer ) return;
	if ( !mappedStaticInstanceBuffer ) return;

	bool lastUseTreeShader = false;
	bool hasBoundAnyShader = false;

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for ( const StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		if ( !group.mesh ) continue;
		if ( group.subMeshIndex >= group.mesh->m_SubMeshes.size() ) continue;

		const SubMesh& sm = group.mesh->m_SubMeshes[group.subMeshIndex];
		if ( sm.indices.empty() ) continue;

		const UINT maxInstanceCount =
			static_cast< UINT >( group.visibleSceneObjectIndices.size() );

		if ( maxInstanceCount == 0 )
			continue;

		const UINT instanceBase = group.instanceBufferStart;

		if ( ( instanceBase + maxInstanceCount ) > m_staticInstanceBufferCapacity )
			continue;

		UINT visibleInstanceCount = 0;

		for ( UINT i = 0; i < maxInstanceCount; ++i )
		{
			const UINT objectIndex = group.visibleSceneObjectIndices[i];

			StaticInstanceVertex& dst =
				mappedStaticInstanceBuffer[instanceBase + visibleInstanceCount];

			if ( !WriteStaticInstanceVertexFromCache(dst, objectIndex) )
				continue;

			++visibleInstanceCount;
		}

		if ( visibleInstanceCount == 0 )
			continue;

		D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
		vbViews[0] = sm.vbView;
		vbViews[1].BufferLocation =
			staticInstanceBuffer->GetGPUVirtualAddress() +
			( UINT64 ) ( sizeof(StaticInstanceVertex) * instanceBase );
		vbViews[1].SizeInBytes = sizeof(StaticInstanceVertex) * visibleInstanceCount;
		vbViews[1].StrideInBytes = sizeof(StaticInstanceVertex);

		if ( !hasBoundAnyShader || lastUseTreeShader != group.useTreeShader )
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

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	ID3D12Resource* skinnedInstanceBuffer =
		m_pd3dSkinnedInstanceBuffer[frameIndex].Get();

	SkinnedInstanceVertex* mappedSkinnedInstanceBuffer =
		m_pMappedSkinnedInstanceBuffer[frameIndex];

	ID3D12Resource* skinnedBonePaletteBuffer =
		m_pd3dSkinnedBonePaletteBuffer[frameIndex].Get();

	XMFLOAT4X4* mappedSkinnedBonePaletteBuffer =
		m_pMappedSkinnedBonePaletteBuffer[frameIndex];

	if ( !skinnedInstanceBuffer ) return;
	if ( !mappedSkinnedInstanceBuffer ) return;
	if ( !skinnedBonePaletteBuffer ) return;
	if ( !mappedSkinnedBonePaletteBuffer ) return;

	cmd->SetGraphicsRootShaderResourceView(
		ROOT_PARAMETER_BONE_PALETTE,
		skinnedBonePaletteBuffer->GetGPUVirtualAddress()
	);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	bool lastUseAlphaClipShader = false;
	bool hasBoundAnyShader = false;

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

			if ( objectIndex < ( UINT ) m_skinnedOcclusionCullFlags.size() )
			{
				if ( m_skinnedOcclusionCullFlags[objectIndex] != 0 )
					continue;
			}

			CGameObject* obj = m_skinnedBatch.objectRefs[objectIndex];
			if ( !obj->GetActive() ) continue;
			if ( !obj ) continue;
			if ( !obj->IsVisible(camera) ) continue;

			auto* renderer = obj->GetComponent<CSkinnedMeshRendererComponent>();
			if ( !renderer ) continue;
			if ( !renderer->IsEnabled() ) continue;

			auto* skin = obj->GetComponent<CSkinningComponent>();
			if ( !skin ) continue;
			if ( !skin->IsSkinned() ) continue;
			const SkinnedComponentCache* cache =
				GetSkinnedComponentCache(objectIndex);

			if ( !cache || !cache->object )
				continue;

			if ( !cache->object->IsVisible(camera) )
				continue;

			SkinnedInstanceVertex& dst =
				mappedSkinnedInstanceBuffer[instanceBase + visibleInstanceCount];

			if ( !WriteSkinnedInstanceVertexFromCache(
				dst,
				*cache,
				objectIndex,
				group.meshIndex,
				group.subMeshIndex,
				mappedSkinnedBonePaletteBuffer) )
			{
				continue;
			}

			++visibleInstanceCount;
		}

		if ( visibleInstanceCount == 0 ) continue;

		if ( !hasBoundAnyShader || lastUseAlphaClipShader != group.useAlphaClipShader )
		{
			if ( group.useAlphaClipShader && m_skinnedAlphaClipShader )
			{
				m_skinnedAlphaClipShader->Render(cmd, camera, &m_skinnedBatch);
			}
			else
			{
				m_skinnedBatch.shader->Render(cmd, camera, &m_skinnedBatch);
			}

			// shader->Render()가 PSO/root state를 만질 수 있으므로 bone palette를 다시 보장.
			cmd->SetGraphicsRootShaderResourceView(
				ROOT_PARAMETER_BONE_PALETTE,
				skinnedBonePaletteBuffer->GetGPUVirtualAddress()
			);

			lastUseAlphaClipShader = group.useAlphaClipShader;
			hasBoundAnyShader = true;
		}

		D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
		vbViews[0] = repSm.vbView;
		vbViews[1].BufferLocation =
			skinnedInstanceBuffer->GetGPUVirtualAddress() +
			( UINT64 ) ( sizeof(SkinnedInstanceVertex) * instanceBase );
		vbViews[1].SizeInBytes = sizeof(SkinnedInstanceVertex) * visibleInstanceCount;
		vbViews[1].StrideInBytes = sizeof(SkinnedInstanceVertex);

		cmd->IASetVertexBuffers(0, 2, vbViews);
		cmd->IASetIndexBuffer(&repSm.ibView);

		cmd->DrawIndexedInstanced(( UINT ) repSm.indices.size(), visibleInstanceCount, 0, 0, 0);
	}
}

void CGameScene::RenderStaticInstanceGroupsToShadowMap(ID3D12GraphicsCommandList* cmd)
{
	PROFILE_RENDER_SCOPE("GameScene::RenderStaticInstanceGroupsToShadowMap");

	if ( !cmd ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	ID3D12Resource* staticInstanceBuffer =
		m_pd3dStaticInstanceBuffer[frameIndex].Get();

	StaticInstanceVertex* mappedStaticInstanceBuffer =
		m_pMappedStaticInstanceBuffer[frameIndex];

	if ( !staticInstanceBuffer ) return;
	if ( !mappedStaticInstanceBuffer ) return;
	if ( !m_shadowStaticShader ) return;
	if ( !m_shadowAlphaClipStaticShader ) return;

	bool lastUseAlphaClipShader = false;
	bool hasBoundAnyShader = false;

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for ( const StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		if ( !group.mesh ) continue;
		if ( group.subMeshIndex >= group.mesh->m_SubMeshes.size() ) continue;

		const SubMesh& sm = group.mesh->m_SubMeshes[group.subMeshIndex];
		if ( sm.indices.empty() ) continue;

		const UINT maxInstanceCount =
			static_cast< UINT >( group.visibleShadowObjectIndices.size() );

		if ( maxInstanceCount == 0 )
			continue;

		// pass 1: shadow
		const UINT instanceBase =
			m_staticInstanceBufferCapacity + group.instanceBufferStart;

		const UINT totalStaticInstanceCapacity =
			m_staticInstanceBufferCapacity * 2;

		if ( ( instanceBase + maxInstanceCount ) > totalStaticInstanceCapacity )
			continue;

		UINT visibleInstanceCount = 0;

		for ( UINT i = 0; i < maxInstanceCount; ++i )
		{
			const UINT objectIndex = group.visibleShadowObjectIndices[i];

			StaticInstanceVertex& dst =
				mappedStaticInstanceBuffer[instanceBase + visibleInstanceCount];

			if ( !WriteStaticInstanceVertexFromCache(dst, objectIndex) )
				continue;

			++visibleInstanceCount;
		}

		if ( visibleInstanceCount == 0 )
			continue;

		if ( !hasBoundAnyShader || lastUseAlphaClipShader != group.useTreeShader )
		{
			if ( group.useTreeShader )
				m_shadowAlphaClipStaticShader->Render(cmd, nullptr, &m_staticBatch);
			else
				m_shadowStaticShader->Render(cmd, nullptr, &m_staticBatch);

			lastUseAlphaClipShader = group.useTreeShader;
			hasBoundAnyShader = true;
		}

		D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
		vbViews[0] = sm.vbView;
		vbViews[1].BufferLocation =
			staticInstanceBuffer->GetGPUVirtualAddress() +
			( UINT64 ) ( sizeof(StaticInstanceVertex) * instanceBase ); 
		vbViews[1].SizeInBytes = sizeof(StaticInstanceVertex) * visibleInstanceCount;
		vbViews[1].StrideInBytes = sizeof(StaticInstanceVertex);

		// opaque shadow는 PS가 없으므로 material binding 불필요.
		// alpha-clip tree shadow만 material/texture 정보가 필요하다.
		if ( group.useTreeShader )
		{
			const UINT mid = ( sm.materialId == 0xFFFFFFFFu ) ? 0u : sm.materialId;
			cmd->SetGraphicsRoot32BitConstant(ROOT_PARAMETER_MATERIAL_ID, mid, 0);

			if ( sm.material && sm.material->NeedsLegacyBinding() )
				sm.material->UpdateShaderVariables(cmd);
		}

		cmd->IASetVertexBuffers(0, 2, vbViews);
		cmd->IASetIndexBuffer(&sm.ibView);

		cmd->DrawIndexedInstanced(( UINT ) sm.indices.size(), visibleInstanceCount, 0, 0, 0);
	}
}

void CGameScene::RenderSkinnedInstanceGroupsToShadowMap(ID3D12GraphicsCommandList* cmd)
{
	if ( !cmd ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	ID3D12Resource* skinnedInstanceBuffer =
		m_pd3dSkinnedInstanceBuffer[frameIndex].Get();

	SkinnedInstanceVertex* mappedSkinnedInstanceBuffer =
		m_pMappedSkinnedInstanceBuffer[frameIndex];

	ID3D12Resource* skinnedBonePaletteBuffer =
		m_pd3dSkinnedBonePaletteBuffer[frameIndex].Get();

	XMFLOAT4X4* mappedSkinnedBonePaletteBuffer =
		m_pMappedSkinnedBonePaletteBuffer[frameIndex];

	if ( !skinnedInstanceBuffer ) return;
	if ( !mappedSkinnedInstanceBuffer ) return;
	if ( !skinnedBonePaletteBuffer ) return;
	if ( !mappedSkinnedBonePaletteBuffer ) return;
	if ( !m_shadowSkinnedShader ) return;
	if ( !m_shadowAlphaClipSkinnedShader ) return;

	cmd->SetGraphicsRootShaderResourceView(
		ROOT_PARAMETER_BONE_PALETTE,
		skinnedBonePaletteBuffer->GetGPUVirtualAddress()
	);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	bool lastUseAlphaClipShader = false;
	bool hasBoundAnyShader = false;

	for ( const SkinnedInstanceGroup& group : m_skinnedInstanceGroups )
	{
		if ( !group.mesh ) continue;
		if ( group.subMeshIndex >= group.mesh->m_SubMeshes.size() ) continue;

		const SubMesh& repSm = group.mesh->m_SubMeshes[group.subMeshIndex];
		if ( repSm.indices.empty() ) continue;

		const UINT maxInstanceCount = ( UINT ) group.objectIndices.size();
		if ( maxInstanceCount == 0 ) continue;

		// pass 1: shadow
		const UINT instanceBase =
			m_skinnedInstanceBufferCapacity + group.instanceBufferStart;

		const UINT totalSkinnedInstanceCapacity =
			m_skinnedInstanceBufferCapacity * 2;

		if ( ( instanceBase + maxInstanceCount ) > totalSkinnedInstanceCapacity )
			continue;

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

			if ( !IsSkinnedObjectInsideShadowBox(objectIndex) )
				continue;

			const SkinnedComponentCache* cache =
				GetSkinnedComponentCache(objectIndex);

			if ( !cache || !cache->object )
				continue;

			SkinnedInstanceVertex& dst =
				mappedSkinnedInstanceBuffer[instanceBase + visibleInstanceCount];

			if ( !WriteSkinnedInstanceVertexFromCache(
				dst,
				*cache,
				objectIndex,
				group.meshIndex,
				group.subMeshIndex,
				mappedSkinnedBonePaletteBuffer) )
			{
				continue;
			}

			++visibleInstanceCount;
		}

		if ( visibleInstanceCount == 0 ) continue;

		if ( !hasBoundAnyShader || ( lastUseAlphaClipShader != group.useAlphaClipShader ) )
		{
			if ( group.useAlphaClipShader )
				m_shadowAlphaClipSkinnedShader->Render(cmd, nullptr, &m_skinnedBatch);
			else
				m_shadowSkinnedShader->Render(cmd, nullptr, &m_skinnedBatch);

			lastUseAlphaClipShader = group.useAlphaClipShader;
			hasBoundAnyShader = true;
		}

		D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
		vbViews[0] = repSm.vbView;
		vbViews[1].BufferLocation =
			skinnedInstanceBuffer->GetGPUVirtualAddress() +
			( UINT64 ) ( sizeof(SkinnedInstanceVertex) * instanceBase );
		vbViews[1].SizeInBytes = sizeof(SkinnedInstanceVertex) * visibleInstanceCount;
		vbViews[1].StrideInBytes = sizeof(SkinnedInstanceVertex);

		cmd->IASetVertexBuffers(0, 2, vbViews);
		cmd->IASetIndexBuffer(&repSm.ibView);

		cmd->DrawIndexedInstanced(( UINT ) repSm.indices.size(), visibleInstanceCount, 0, 0, 0);
	}
}

void CGameScene::RenderDepthFog(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	m_depthFog.Render(cmd, camera);
}

bool CGameScene::IsStaticObjectInsideShadowBox(UINT objectIndex) const
{
	if ( objectIndex >= ( UINT ) m_staticShadowOcclusionEntryIndices.size() )
		return true;

	const int entryIndex = m_staticShadowOcclusionEntryIndices[objectIndex];

	if ( entryIndex < 0 )
		return true;

	if ( entryIndex >= ( int ) m_staticOcclusionEntries.size() )
		return true;

	const StaticOcclusionEntry& entry =
		m_staticOcclusionEntries[( size_t ) entryIndex];

	if ( !entry.hasWorldBounds )
		return true;

	return m_shadowMap.IsWorldOOBBInsideShadowBox(entry.worldBounds);
}

bool CGameScene::IsSkinnedObjectInsideShadowBox(UINT objectIndex) const
{
	if ( objectIndex >= ( UINT ) m_skinnedShadowOcclusionEntryIndices.size() )
		return true;

	const int entryIndex = m_skinnedShadowOcclusionEntryIndices[objectIndex];

	if ( entryIndex < 0 )
		return true;

	if ( entryIndex >= ( int ) m_skinnedOcclusionEntries.size() )
		return true;

	const SkinnedOcclusionEntry& entry =
		m_skinnedOcclusionEntries[( size_t ) entryIndex];

	if ( !entry.hasWorldBounds )
		return true;

	return m_shadowMap.IsWorldOOBBInsideShadowBox(entry.worldBounds);
}

void CGameScene::RenderShadowMap(ID3D12GraphicsCommandList* cmd)
{
	PROFILE_RENDER_SCOPE("GameScene::RenderShadowMap");

	if ( !cmd ) return;
	if ( !m_shadowMap.IsReady() ) return;
	if ( !m_shadowStaticShader ) return;
	if ( !m_shadowSkinnedShader ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	const D3D12_GPU_VIRTUAL_ADDRESS materialCbGpuAddress =
		m_pd3dcbMaterials[frameIndex]
		? m_pd3dcbMaterials[frameIndex]->GetGPUVirtualAddress()
		: 0;

	const bool begun =
		m_shadowMap.BeginRender(
			cmd,
			GetGraphicsRootSignature(),
			m_pDescriptorHeap.get(),
			materialCbGpuAddress
		);

	if ( !begun ) return;

	RenderStaticInstanceGroupsToShadowMap(cmd);
	RenderSkinnedInstanceGroupsToShadowMap(cmd);

	m_shadowMap.EndRender(cmd);
}

void CGameScene::RestoreSceneRenderTargets(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !m_bSceneRenderTargetsReady ) return;
	if ( m_sceneRenderTargetCount == 0 ) return;

	cmd->OMSetRenderTargets(
		m_sceneRenderTargetCount,
		m_sceneRtvHandles.data(),
		FALSE,
		&m_sceneDsvHandle
	);

	if ( camera )
		camera->SetViewportsAndScissorRects(cmd);
}

void CGameScene::RenderShadowPrePass(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd )
		return;

	{
		PROFILE_RENDER_SCOPE("GameScene::RenderShadowPrePass::PrepareFrame");

		CScene::OnPrepareRender(cmd, camera);

		UpdateFrameRenderState(camera);

		UpdateShaderVariables(cmd);

		BindFrameRootParameters(cmd);
		{
			PROFILE_RENDER_SCOPE("GameScene::RenderShadowPrePass::BuildShadowVisibleLists");
			BuildStaticShadowVisibleListsForFrame();
		}
	}
	RenderShadowMap(cmd);
	RestoreSceneRenderTargets(cmd, camera);
}

bool CGameScene::GetPauseOverlayRect(XMFLOAT4& outRect) const
{
	return m_hud.GetPauseOverlayRect(outRect);
}

bool CGameScene::IsPointInPauseOverlay(POINT clientPt) const
{
	return m_hud.IsPointInPauseOverlay(clientPt);
}

bool CGameScene::IsPointInResumeButton(POINT clientPt) const
{
	return m_hud.IsPointInResumeButton(clientPt);
}

bool CGameScene::IsPointInExitButton(POINT clientPt) const
{
	return m_hud.IsPointInExitButton(clientPt);
}

void CGameScene::SetMaterialDiffuseSrvIndex(int materialId, UINT srvIndex)
{
	if ( !m_pMaterials )
		return;

	if ( materialId < 0 || materialId >= MAX_MATERIALS )
		return;

	m_pMaterials->m_pReflections[materialId].m_xmn4TextureIndices.x =
		( srvIndex == UINT_MAX ) ? 0u : ( srvIndex + 1u );
}

void CGameScene::SetKeyItemDiffuseSrvIndex(UINT srvIndex)
{
	SetMaterialDiffuseSrvIndex(( int ) kItemBillboardKeyMaterialId, srvIndex);
}

void CGameScene::SetTransparentItemDiffuseSrvIndex(UINT srvIndex)
{
	SetMaterialDiffuseSrvIndex(
		static_cast< int >( kTransparentItemBillboardMaterialId ),
		srvIndex
	);
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

	if ( slot == m_localPlayerSlot && m_bLocalPlayerDead )
		return;

	constexpr float kArrowPullBackDistance = 0.35f;
	constexpr float kBulletSpeed = 25.0f;
	constexpr float kBulletLife = 3.0f;

	bool shouldPlaySwordWhoosh = false;
	bool shouldPlayAxeWhoosh = false;
	bool shouldPrepareArrow = false;
	bool shouldFireBullet = false;

	CPlayerEquipmentComponent* equipComp = nullptr;

	if ( auto* equip = obj->GetComponent<CPlayerEquipmentComponent>() )
	{
		equipComp = equip;

		const EWeaponType weapon = equip->GetEquippedWeapon();

		shouldPlaySwordWhoosh = ( weapon == EWeaponType::Sword );
		shouldPlayAxeWhoosh = ( weapon == EWeaponType::Axe );
		shouldPrepareArrow = ( weapon == EWeaponType::Bow );
		shouldFireBullet = ( weapon == EWeaponType::Gun );
	}

	if ( auto* animComp = obj->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->EnsureController() )
		{
			const bool accepted = ctrl->RequestAttack();

			if ( accepted && equipComp )
			{
				if ( shouldPlaySwordWhoosh )
				{
					equipComp->RequestSwordAttackWhoosh();
					BeginSwordTrail(obj);
				}
				else if ( shouldPlayAxeWhoosh )
				{
					equipComp->RequestAxeAttackWhoosh();
					BeginAxeTrail(obj);
				}
			}

			if ( accepted && shouldPrepareArrow )
				RequestPrepareArrow(obj, kArrowPullBackDistance);

			if ( accepted && shouldFireBullet )
			{
				if ( equipComp )
					equipComp->RequestGunShotSfx();

				RequestFireBullet(obj, kBulletSpeed, kBulletLife);
			}

			return;
		}
	}

	if ( auto* ctrl = obj->GetAnimController() )
	{
		const bool accepted = ctrl->RequestAttack();

		if ( accepted && equipComp )
		{
			if ( shouldPlaySwordWhoosh )
			{
				equipComp->RequestSwordAttackWhoosh();
				BeginSwordTrail(obj);
			}
			else if ( shouldPlayAxeWhoosh )
			{
				equipComp->RequestAxeAttackWhoosh();
				BeginAxeTrail(obj);
			}
		}

		if ( accepted && shouldPrepareArrow )
			RequestPrepareArrow(obj, kArrowPullBackDistance);

		if ( accepted && shouldFireBullet )
		{
			if ( equipComp )
				equipComp->RequestGunShotSfx();

			RequestFireBullet(obj, kBulletSpeed, kBulletLife);
		}

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

int CGameScene::GetSwordManIndexFromObject(const CGameObject* obj) const
{
	if ( !obj ) return -1;

	for ( size_t i = 0; i < m_swordManRefs.size(); ++i )
	{
		if ( m_swordManRefs[i] == obj )
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

		SetObjectAttackPower(arrowObj, kAttackPowerPlayerArrow);

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

		SetObjectAttackPower(arrowObj, kAttackPowerEnemyArrow);

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
	constexpr float kArrowSpeed = 16.0f;
	constexpr float kArrowLife = 6.0f;

	constexpr float kEnemyArrowPullBackDistance = 0.35f * 1.5f;
	constexpr float kEnemyArrowSpeed = 14.0f;
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

		const size_t slotIndex = static_cast< size_t >( slot );

		if ( hasBowEquipped && isBowLoad && !m_prevBowLoadPhase[slotIndex] )
		{
			if ( auto* equip = player ? player->GetComponent<CPlayerEquipmentComponent>() : nullptr )
			{
				equip->RequestBowLoadingSfx();

				// 릴리즈 사운드는 릴리즈 phase에서 틀지 않고,
				// 로딩 phase 시작 시점에 미리 예약한다.
				equip->RequestBowReleaseSfxFromLoadPhase();
			}
		}

		if ( hasBowEquipped && isBowRelease && !m_prevBowReleasePhase[slotIndex] )
		{
			// 사운드는 이미 Bow_Load 진입 시 예약했으므로 여기서는 화살만 발사.
			RequestReleasePreparedArrow(player, kArrowSpeed, kArrowLife);
		}

		// 공격이 끝났거나 장비가 바뀌면 준비 화살 정리
		if ( ( !hasBowEquipped || ( !isBowLoad && !isBowRelease ) ) && m_preparedPlayerArrows[slotIndex] )
		{
			if ( auto* arrow = m_preparedPlayerArrows[slotIndex]->GetComponent<CArrowComponent>() )
			{
				arrow->Deactivate();
			}
			m_preparedPlayerArrows[slotIndex] = nullptr;
		}

		m_prevBowLoadPhase[slotIndex] = isBowLoad;
		m_prevBowReleasePhase[slotIndex] = isBowRelease;
    }
	for ( size_t i = 0; i < m_bowManRefs.size(); ++i )
	{
		CGameObject* bowman = m_bowManRefs[i];
		if ( IsMonsterDead(bowman) )
		{
			if ( i < m_preparedBowmanArrows.size() && m_preparedBowmanArrows[i] )
			{
				if ( auto* arrow = m_preparedBowmanArrows[i]->GetComponent<CArrowComponent>() )
					arrow->Deactivate();

				m_preparedBowmanArrows[i] = nullptr;
			}

			if ( i < m_prevEnemyBowReleasePhase.size() )
				m_prevEnemyBowReleasePhase[i] = false;

			continue;
		}

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
	if ( !m_sceneGrid.IsInitialized() )
		return false;

	CGameObject* localPlayer = GetPlayer();

	if ( !localPlayer )
		localPlayer = GetPlayerBySlot(0);

	if ( !localPlayer )
		return false;

	const XMFLOAT3 pos = localPlayer->GetPosition();

	int cellX = -1;
	int cellZ = -1;

	if ( !m_sceneGrid.WorldToCell(pos.x, pos.z, cellX, cellZ) )
		return false;

	int megaX = -1;
	int megaZ = -1;

	if ( !m_sceneGrid.FineCellToMegaGridCell(cellX, cellZ, megaX, megaZ) )
		return false;

	if ( megaX == kCastleCenterMegaGridX &&
		 megaZ == kCastleCenterMegaGridZ )
	{
#ifdef USING_NETWORK
		return true;
#else
		return m_bLocalPlayerInsideCastleCenterMegaGrid;
#endif
	}

	return m_sceneGrid.IsFineCellInsideMegaGridApproachZone(
		megaX,
		megaZ,
		cellX,
		cellZ
	);
}

int CGameScene::GetLocalPlayerMegaGridNumberForDepthFog() const
{
	if ( !m_sceneGrid.IsInitialized() )
		return -1;

	if ( m_localPlayerSlot < 0 ||
		 m_localPlayerSlot >= static_cast< int >(m_playerGridTrackers.size()) )
	{
		return -1;
	}

	const GridDynamicTracker& tracker =
		m_playerGridTrackers[static_cast< size_t >(m_localPlayerSlot)];

	if ( !tracker.occupied )
		return -1;

	return m_sceneGrid.MegaGridNumberFromCell(tracker.prevCellX, tracker.prevCellZ);
}

void CGameScene::UpdateDepthFogState(float dt)
{
	const int megaGridNumber = GetLocalPlayerMegaGridNumberForDepthFog();

	const bool isDenseFogZone =
		( megaGridNumber == 1 || megaGridNumber == 9 );

	const EDepthFogPresetMode fogMode =
		isDenseFogZone
		? EDepthFogPresetMode::ZoneDense
		: EDepthFogPresetMode::OuterWide;

	m_depthFog.UpdateState(dt, fogMode);
}

bool CGameScene::IsLocalPlayer(const CGameObject* obj) const
{
    if (!obj) return false;
    auto* tag = obj->GetComponent<CActorTagComponent>();
    return tag && tag->kind == EActorKind::Player && tag->control == EPlayerControl::Local;
}

void CGameScene::SetLocalPlayerControlEnabled(bool enabled)
{
	CGameObject* player = GetPlayer();
	if ( !player )
		return;

	if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
	{
		controller->SetInputEnabled(enabled);

		if ( !enabled )
		{
			controller->SetInputDirection(static_cast< DWORD >( 0 ));
			controller->SetRunRequested(false);
			controller->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
		}
	}
}

void CGameScene::CancelLocalPlayerPreparedActions()
{
	const int slot = m_localPlayerSlot;

	if ( slot < 0 || slot >= 4 )
		return;

	if ( m_preparedPlayerArrows[( size_t ) slot] )
	{
		if ( auto* arrow = m_preparedPlayerArrows[( size_t ) slot]->GetComponent<CArrowComponent>() )
			arrow->Deactivate();

		m_preparedPlayerArrows[( size_t ) slot] = nullptr;
	}

	m_prevBowReleasePhase[( size_t ) slot] = false;

	auto DisableColliderOnly = [ ] (CGameObject* obj)
		{
			if ( !obj )
				return;

			if ( auto* collider = obj->GetComponent<CColliderComponent>() )
				collider->SetEnabled(false);
		};

	if ( slot >= 0 && slot < ( int ) m_PlayerSwordRefs.size() )
		DisableColliderOnly(m_PlayerSwordRefs[( size_t ) slot]);

	if ( slot >= 0 && slot < ( int ) m_PlayerAxeRefs.size() )
		DisableColliderOnly(m_PlayerAxeRefs[( size_t ) slot]);

	if ( slot >= 0 && slot < ( int ) m_PlayerGunRefs.size() )
		DisableColliderOnly(m_PlayerGunRefs[( size_t ) slot]);
}

bool CGameScene::IsMonsterDead(const CGameObject* monster) const
{
	if ( !monster )
		return true;

	if ( m_deadMonsters.find(const_cast< CGameObject* >(monster)) != m_deadMonsters.end() )
		return true;

	if ( auto* hp = monster->GetComponent<CHealthComponent>() )
		return hp->IsDead();

	return false;
}

void CGameScene::MarkMegaGridClearedByNumber(int megaGridNumber)
{
	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return;

	const int zeroBased = megaGridNumber - 1;
	const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
	const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

	if ( m_sceneGrid.IsMegaGridCleared(megaX, megaZ) )
		return;

	m_sceneGrid.SetMegaGridCleared(megaX, megaZ, true);
}

bool CGameScene::AreAllMonstersInMegaGridDead(int megaGridNumber) const
{
	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return false;

	const int zeroBased = megaGridNumber - 1;
	const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
	const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

	const std::vector<CGameObject*>& monsters =
		m_sceneGrid.GetMegaGridMonsters(megaX, megaZ);

	if ( monsters.empty() )
		return false;

	for ( const CGameObject* monster : monsters )
	{
		if ( !IsMonsterDead(monster) )
			return false;
	}

	return true;
}

void CGameScene::RegisterMutantKeyTriggerIfNeeded(
	CGameObject* mutant,
	int megaGridNumber)
{
	if ( !mutant )
		return;

	if ( megaGridNumber != 6 && megaGridNumber != 8 )
		return;

	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return;

	if ( m_mutantKeyTriggerRegisteredByMega[( size_t ) megaGridNumber] )
		return;

	m_mutantKeyTriggerRegisteredByMega[( size_t ) megaGridNumber] = true;
	m_mutantKeyTriggerMegaByObject[mutant] = megaGridNumber;

	char buf[256];
	sprintf_s(
		buf,
		"[MutantKeyTrigger] Registered first Mutant for mega grid %d. mutant=%p\n",
		megaGridNumber,
		static_cast< void* >( mutant )
	);
	OutputDebugStringA(buf);
}

void CGameScene::UnlockKeyBillboardForMegaGrid(int megaGridNumber)
{
	if ( megaGridNumber != 6 && megaGridNumber != 8 )
		return;

	for ( ItemBillboardEntry& item : m_itemBillboards )
	{
		if ( item.kind != EItemBillboardKind::Key )
			continue;

		if ( item.megaGridNumber != megaGridNumber )
			continue;

		// 이미 먹은 열쇠는 다시 살리지 않는다.
		if ( m_sceneGrid.IsMegaGridCleared(
			( megaGridNumber - 1 ) % CSceneGrid::kMegaGridCols,
			( megaGridNumber - 1 ) / CSceneGrid::kMegaGridCols) )
		{
			return;
		}

		item.active = true;
		item.distanceCulled = false;

		char buf[256];
		sprintf_s(
			buf,
			"[MutantKeyTrigger] Key billboard unlocked for mega grid %d.\n",
			megaGridNumber
		);
		OutputDebugStringA(buf);

		return;
	}
}

void CGameScene::HandleMutantKeyTriggerDeath(CGameObject* monster)
{
	if ( !monster )
		return;

	const auto it = m_mutantKeyTriggerMegaByObject.find(monster);
	if ( it == m_mutantKeyTriggerMegaByObject.end() )
		return;

	const int megaGridNumber = it->second;

	UnlockKeyBillboardForMegaGrid(megaGridNumber);

	m_mutantKeyTriggerMegaByObject.erase(it);
}

void CGameScene::UpdateMegaGridClearStateFromMonsterDeaths()
{
	// 2번 메가그리드: 해당 메가그리드의 모든 몬스터 사망 시 클리어.
	if ( !m_sceneGrid.IsMegaGridCleared(1, 0) )
	{
		if ( AreAllMonstersInMegaGridDead(2) )
			MarkMegaGridClearedByNumber(2);
	}

	// 5번 메가그리드: 보스 사망 시 클리어.
	if ( !m_sceneGrid.IsMegaGridCleared(1, 1) )
	{
		for ( const CGameObject* boss : m_bossRefs )
		{
			if ( IsMonsterDead(boss) )
			{
				MarkMegaGridClearedByNumber(5);
				break;
			}
		}
	}
}

void CGameScene::CancelMonsterPreparedActions(CGameObject* monster)
{
	if ( !monster )
		return;

	// ---------------------------------------------------------------------
	// 1) BowMan이 Bow_Load 중 죽은 경우 준비 중인 화살 제거
	// ---------------------------------------------------------------------
	const int bowmanIndex = GetBowManIndexFromObject(monster);
	if ( bowmanIndex >= 0 )
	{
		const size_t idx = static_cast< size_t >( bowmanIndex );

		if ( idx < m_preparedBowmanArrows.size() )
		{
			if ( m_preparedBowmanArrows[idx] )
			{
				if ( auto* arrow = m_preparedBowmanArrows[idx]->GetComponent<CArrowComponent>() )
					arrow->Deactivate();

				m_preparedBowmanArrows[idx] = nullptr;
			}
		}

		if ( idx < m_prevEnemyBowReleasePhase.size() )
			m_prevEnemyBowReleasePhase[idx] = false;
	}

	// ---------------------------------------------------------------------
	// 2) SwordMan의 외부 무기 collider 비활성화
	//    m_swordManRefs와 m_EnemySwordRefs는 같은 순서로 연결되어 있음.
	// ---------------------------------------------------------------------
	for ( size_t i = 0; i < m_swordManRefs.size(); ++i )
	{
		if ( m_swordManRefs[i] != monster )
			continue;

		if ( i < m_EnemySwordRefs.size() )
		{
			CGameObject* sword = m_EnemySwordRefs[i];

			if ( sword )
			{
				if ( auto* collider = sword->GetComponent<CColliderComponent>() )
					collider->SetEnabled(false);

				if ( auto* hitbox = sword->GetComponent<CMonsterWeaponHitboxComponent>() )
					hitbox->SetEnabled(false);
			}
		}

		break;
	}

	// ---------------------------------------------------------------------
	// 3) Ghoul / Mutant / Boss처럼 owner bone weapon capsule을 쓰는 경우
	// ---------------------------------------------------------------------
	if ( auto* ownerWeaponHitbox = monster->GetComponent<CMonsterWeaponHitboxComponent>() )
		ownerWeaponHitbox->SetEnabled(false);
}

void CGameScene::BeginMonsterDeath(CGameObject* monster)
{
	if ( !monster )
		return;

	if ( m_deadMonsters.find(monster) != m_deadMonsters.end() )
		return;

	auto* tag = monster->GetComponent<CActorTagComponent>();
	if ( !tag || tag->kind != EActorKind::NPC )
		return;

	m_deadMonsters.insert(monster);

	HandleMutantKeyTriggerDeath(monster);

	CancelMonsterPreparedActions(monster);

	// 더 이상 플레이어 무기 충돌을 받지 않게 함.
	if ( auto* collider = monster->GetComponent<CColliderComponent>() )
		collider->SetEnabled(false);

	// 현재 실제로 붙는 AI는 CGhoulAIComponent지만,
	// base 타입으로도 잡히는 구조라면 같이 처리.
	if ( auto* ai = monster->GetComponent<CMonsterAIComponent>() )
	{
		ai->SetEnabledAI(false);
		ai->ClearTarget();
		ai->ClearPath();
	}

	if ( auto* ghoulAI = monster->GetComponent<CGhoulAIComponent>() )
	{
		ghoulAI->SetEnabledAI(false);
		ghoulAI->ClearTarget();
		ghoulAI->ClearPath();
	}

	if ( auto* animComp = monster->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->EnsureMonsterController() )
		{
			ctrl->SetLocomotionState(EMonsterAnimState::Idle);
			ctrl->RequestCommand(EMonsterAnimCommand::Death);
			return;
		}
	}
}

void CGameScene::UpdateMonsterDeathStates()
{
	for ( const SkinnedComponentCache& cache : m_skinnedComponentCache )
	{
		CGameObject* obj = cache.object;
		if ( !obj )
			continue;

		if ( !cache.isNpc )
			continue;

		if ( !cache.health )
			continue;

		if ( cache.health->IsDead() )
			BeginMonsterDeath(obj);
	}
}

void CGameScene::BeginLocalPlayerDeath(CGameObject* player)
{
	if ( !player )
		return;

	if ( m_bLocalPlayerDead )
		return;

	m_bLocalPlayerDead = true;
	m_localPlayerRespawnTimer = 0.0f;

	SetLocalPlayerControlEnabled(false);
	CancelLocalPlayerPreparedActions();

	if ( auto* collider = player->GetComponent<CColliderComponent>() )
		collider->SetEnabled(false);

	if ( auto* animComp = player->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->EnsureController() )
		{
			ctrl->RequestDeath();
			return;
		}
	}

	if ( auto* ctrl = player->GetAnimController() )
		ctrl->RequestDeath();
}

void CGameScene::RespawnLocalPlayer(CGameObject* player)
{
	if ( !player )
		return;

	player->SetPosition(kLocalPlayerRespawnPosition);

	if ( auto* hp = player->GetComponent<CHealthComponent>() )
		hp->ResetToMax();

	if ( auto* collider = player->GetComponent<CColliderComponent>() )
	{
		collider->SetEnabled(true);
		collider->UpdateWorldBounds();
	}

	if ( auto* animComp = player->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->EnsureController() )
		{
			ctrl->ResetToIdleAfterRespawn();
		}
	}
	else if ( auto* ctrl = player->GetAnimController() )
	{
		ctrl->ResetToIdleAfterRespawn();
	}

	SetLocalPlayerControlEnabled(true);

	m_bLocalPlayerDead = false;
	m_bLocalPlayerRespawnUsed = true;
	m_localPlayerRespawnTimer = 0.0f;

	UpdateDynamicGridState();
}

void CGameScene::UpdateLocalPlayerDeathAndRespawn(float dt)
{
	CGameObject* player = GetPlayer();
	if ( !player )
		return;

	auto* hp = player->GetComponent<CHealthComponent>();
	if ( !hp )
		return;

	if ( !m_bLocalPlayerDead && hp->IsDead() )
	{
		BeginLocalPlayerDeath(player);
	}

	if ( !m_bLocalPlayerDead )
		return;

	if ( m_bLocalPlayerRespawnUsed )
		return;

	if ( dt > 0.0f )
		m_localPlayerRespawnTimer += dt;

	if ( m_localPlayerRespawnTimer >= kLocalPlayerRespawnDelay )
	{
		RespawnLocalPlayer(player);
	}
}

bool CGameScene::RollbackLocalPlayerMoveIfCollidingWorldStatic(const XMFLOAT3& previousPos)
{
	if ( !m_bSimulateLocalPlayerWorldStaticRollback )
		return false;

	CGameObject* localPlayer = GetPlayer();
	if ( !localPlayer ) return false;
	if ( !m_Collision ) return false;

	auto* collider = localPlayer->GetComponent<CColliderComponent>();
	if ( !collider ) return false;

	const XMFLOAT3 currentPos = localPlayer->GetPosition();

	auto TestPositionAgainstWorldStatic = [ this, localPlayer, collider ] (const XMFLOAT3& testPos) -> bool
		{
			localPlayer->SetPosition(testPos);
			collider->UpdateWorldBounds();
			return m_Collision->HasCollisionWithWorldStatic(collider);
		};

	const bool hitWorldStaticAtCurrentPos = TestPositionAgainstWorldStatic(currentPos);

	// 현재 위치가 애초에 안 겹치면 아무 것도 안 함
	if ( !hitWorldStaticAtCurrentPos )
	{
#ifndef USING_NETWORK
		if ( kEnableTowerDoorPortalVerboseLog )
		{
			char buf[256];
			sprintf_s(
				buf,
				"[TowerDoorPortal][ROLLBACK_CHECK] no world-static hit pos=(%.3f, %.3f, %.3f)\n",
				currentPos.x,
				currentPos.y,
				currentPos.z
			);
			OutputDebugStringA(buf);
		}
#endif
		return false;
	}

#ifndef USING_NETWORK
	if ( kEnableTowerDoorPortalCollisionLog )
	{
		char buf[512];
		sprintf_s(
			buf,
			"[TowerDoorPortal][WORLD_STATIC_HIT] pos=(%.3f, %.3f, %.3f) prev=(%.3f, %.3f, %.3f) portalCount=%zu\n",
			currentPos.x,
			currentPos.y,
			currentPos.z,
			previousPos.x,
			previousPos.y,
			previousPos.z,
			m_towerDoorPortals.size()
		);
		OutputDebugStringA(buf);
	}

	localPlayer->SetPosition(currentPos);
	collider->UpdateWorldBounds();

	if ( m_bSimulateLocalTeleport )
	{
		if ( TryTeleportLocalPlayerByTowerDoorPortal(true) )
			return true;

		if ( TryTeleportLocalPlayerByCastleDoorPortal(true) )
			return true;
	}
#endif

#ifndef USING_NETWORK
	if ( m_bSimulateLocalTeleport && IsTowerDoorPortalOnCooldown() )
	{
		localPlayer->SetPosition(currentPos);
		collider->UpdateWorldBounds();
		return true;
	}
#endif

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
		collider->UpdateWorldBounds();
		return true;
	}

	if ( xResolved )
	{
		localPlayer->SetPosition(candidateRollbackX);
		collider->UpdateWorldBounds();
		return true;
	}

	if ( zResolved )
	{
		localPlayer->SetPosition(candidateRollbackZ);
		collider->UpdateWorldBounds();
		return true;
	}

	// 둘 다 안 되면 전체 롤백
	localPlayer->SetPosition(previousPos);
	collider->UpdateWorldBounds();
	return true;
}

bool CGameScene::OnProcessingMouseMessage(HWND /*hWnd*/, UINT msg, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	if ( msg == WM_LBUTTONDOWN )
	{
#ifndef USING_NETWORK
		if ( !m_bLocalPlayerDead )
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

		SetObjectAttackPower(bulletObj, kAttackPowerPlayerBullet);

		if ( bullet->FireFromObjects(spawnSource, directionSource, speed, lifeSec, true) )
		{
			const XMFLOAT3 dirN = GetSafeObjectForward(directionSource);

			XMFLOAT3 muzzlePos = spawnSource->GetPosition();

			if ( gunObj )
			{
				muzzlePos.x += dirN.x * 0.55f;
				muzzlePos.y += 0.05f;
				muzzlePos.z += dirN.z * 0.55f;
			}
			else
			{
				muzzlePos.y += 1.15f;
				muzzlePos.x += dirN.x * 0.85f;
				muzzlePos.z += dirN.z * 0.85f;
			}

			SpawnMuzzleFlash(muzzlePos, dirN);

			return;
		}
	}
}

bool CGameScene::ProcessInput(UCHAR* pKeysBuffer)
{
#ifndef USING_NETWORK
	if ( !pKeysBuffer )
	{
		m_bPrevLocalMonsterChaseToggleKeyDown = false;
		m_bPrevLocalStageTeleportKeyDown.fill(false);
		return false;
	}

	// ---------------------------------------------------------------------
	// Q: 로컬 몬스터 추적 on/off
	// ---------------------------------------------------------------------
	const bool qDown = ( pKeysBuffer['Q'] & 0xF0 ) != 0;

	if ( qDown && !m_bPrevLocalMonsterChaseToggleKeyDown )
	{
		SetLocalMonsterChaseEnabled(!m_bSimulateLocalMonsterChase);
		m_bPrevLocalMonsterChaseToggleKeyDown = true;
		return true;
	}

	m_bPrevLocalMonsterChaseToggleKeyDown = qDown;

	// ---------------------------------------------------------------------
	// 1~9: 로컬 스테이지 메가그리드 강제 텔레포트
	//
	// 배치:
	// 789
	// 456
	// 123
	//
	// false이면 입력 상태만 갱신하고 실제 텔레포트는 하지 않는다.
	// ---------------------------------------------------------------------
	for ( int megaGridNumber = 1;
		  megaGridNumber <= CSceneGrid::kMegaGridCount;
		  ++megaGridNumber )
	{
		const bool down =
			( pKeysBuffer['0' + megaGridNumber] & 0xF0 ) != 0;

		const bool prevDown =
			m_bPrevLocalStageTeleportKeyDown[
				static_cast< size_t >( megaGridNumber )
			];

		if ( down && !prevDown )
		{
			m_bPrevLocalStageTeleportKeyDown[
				static_cast< size_t >( megaGridNumber )
			] = true;

			if ( m_bSimulateLocalStageTeleport )
			{
				return TryTeleportLocalPlayerToMegaGridByNumber(megaGridNumber);
			}

			return false;
		}

		m_bPrevLocalStageTeleportKeyDown[
			static_cast< size_t >( megaGridNumber )
		] = down;
	}
#else
	UNREFERENCED_PARAMETER(pKeysBuffer);
#endif

	return false;
}

bool CGameScene::ShouldEvaluateSkinnedPoseThisFrame(UINT objectIndex, CCamera* camera) const
{
	const SkinnedComponentCache* cache = GetSkinnedComponentCache(objectIndex);

	if ( !cache || !cache->object )
		return false;

	CGameObject* obj = cache->object;

	// 비네트워크 모드는 클라이언트가 실제 판정도 하므로 보수적으로 full pose 유지.
#ifndef USING_NETWORK
	return true;
#else
	// 로컬 플레이어는 항상 full pose 유지.
	if ( IsLocalPlayer(obj) )
		return true;

	if ( !cache->renderer || !cache->renderer->IsEnabled() )
		return false;

	const bool distanceCulled =
		( objectIndex < static_cast< UINT >(m_skinnedDistanceCullFlags.size()) ) &&
		( m_skinnedDistanceCullFlags[objectIndex] != 0 );

	if ( distanceCulled )
		return false;

	const bool occlusionCulled =
		( objectIndex < static_cast< UINT >(m_skinnedOcclusionCullFlags.size()) ) &&
		( m_skinnedOcclusionCullFlags[objectIndex] != 0 );

	const bool frustumVisible =
		( camera == nullptr ) || obj->IsVisible(camera);

	const bool sceneVisible =
		!occlusionCulled && frustumVisible;

	// Shadow pass에서도 렌더될 수 있으면 pose를 갱신한다.
	const bool shadowVisible =
		IsSkinnedObjectInsideShadowBox(objectIndex);

	return sceneVisible || shadowVisible;
#endif
}

#ifdef USING_NETWORK
void CGameScene::PushNetworkFrameSnapshot(const FrameSnapshot& snapshot)
{
	if (snapshot.frameId <= m_lastReceivedServerTick)
		return;

	m_lastReceivedServerTick = snapshot.frameId;
	m_frameSnapshotBuffer.push_back(snapshot);

	while (m_frameSnapshotBuffer.size() > kMaxNetworkFrameSnapshotBufferSize)
		m_frameSnapshotBuffer.pop_front();
}

FrameSnapshot CGameScene::BuildInterpolatedFrameSnapshot(const FrameSnapshot& latestSnapshot) const
{
	FrameSnapshot displaySnapshot = latestSnapshot;

	if (m_lastReceivedServerTick <= kNetworkInterpolationDelayTicks)
		return displaySnapshot;

	const uint64_t renderTick = m_lastReceivedServerTick - kNetworkInterpolationDelayTicks;

	const FrameSnapshot* older = nullptr;
	const FrameSnapshot* newer = nullptr;
	float alpha = 0.0f;
	if (!GetInterpolationSnapshots(renderTick, older, newer, alpha) || !older || !newer)
		return displaySnapshot;

	for (PlayerState& state : displaySnapshot.players)
	{
		if (static_cast<int>(state.id) == m_localPlayerSlot)
			continue;

		const PlayerState* oldState = FindPlayerState(*older, state.id);
		const PlayerState* newState = FindPlayerState(*newer, state.id);
		if (!oldState || !newState)
			continue;

		state.position = LerpPosition(oldState->position, newState->position, alpha);
		state.yaw = LerpYawDegrees(oldState->yaw, newState->yaw, alpha);
	}

	for (EnemyState& state : displaySnapshot.enemies)
	{
		const EnemyState* oldState = FindEnemyState(*older, state.id);
		const EnemyState* newState = FindEnemyState(*newer, state.id);
		if (!oldState || !newState)
			continue;

		state.position = LerpPosition(oldState->position, newState->position, alpha);
		state.yaw = LerpYawDegrees(oldState->yaw, newState->yaw, alpha);
	}

	return displaySnapshot;
}

bool CGameScene::GetInterpolationSnapshots(
	uint64_t renderTick,
	const FrameSnapshot*& older,
	const FrameSnapshot*& newer,
	float& alpha) const
{
	older = nullptr;
	newer = nullptr;
	alpha = 0.0f;

	if (m_frameSnapshotBuffer.size() < 2)
		return false;

	for (size_t i = 1; i < m_frameSnapshotBuffer.size(); ++i)
	{
		const FrameSnapshot& a = m_frameSnapshotBuffer[i - 1];
		const FrameSnapshot& b = m_frameSnapshotBuffer[i];
		if (a.frameId > renderTick || renderTick > b.frameId)
			continue;

		older = &a;
		newer = &b;

		const uint64_t tickSpan = b.frameId - a.frameId;
		if (tickSpan > 0)
			alpha = static_cast<float>(renderTick - a.frameId) / static_cast<float>(tickSpan);

		if (alpha < 0.0f) alpha = 0.0f;
		if (alpha > 1.0f) alpha = 1.0f;
		return true;
	}

	return false;
}

const PlayerState* CGameScene::FindPlayerState(const FrameSnapshot& snapshot, uint64_t id)
{
	for (const PlayerState& state : snapshot.players)
	{
		if (state.id == id)
			return &state;
	}
	return nullptr;
}

const EnemyState* CGameScene::FindEnemyState(const FrameSnapshot& snapshot, uint64_t id)
{
	for (const EnemyState& state : snapshot.enemies)
	{
		if (state.id == id)
			return &state;
	}
	return nullptr;
}

XMFLOAT3 CGameScene::LerpPosition(const XMFLOAT3& a, const XMFLOAT3& b, float t)
{
	return XMFLOAT3(
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t);
}

float CGameScene::LerpYawDegrees(float a, float b, float t)
{
	float delta = b - a;
	while (delta > 180.0f) delta -= 360.0f;
	while (delta < -180.0f) delta += 360.0f;

	float result = a + delta * t;
	while (result >= 360.0f) result -= 360.0f;
	while (result < 0.0f) result += 360.0f;
	return result;
}
#endif

void CGameScene::AnimateObjects(float dt)
{
	m_fElapsedTime = dt;

	CGameObject* local = GetPlayer();
	if ( !local )
		local = GetPlayerBySlot(0);

#ifndef USING_NETWORK
	if ( m_bSimulateLocalEnemySpawner && m_enemySpawner && local )
	{
		m_enemySpawner->Update(dt, local->GetPosition());
	}
#endif

	UpdateMuzzleFlashes(dt);
	UpdateSwordTrails(dt);
	UpdateMonsterSwordTrails(dt);

	UpdateMonsterDeathStates();

#ifndef USING_NETWORK
	UpdateLocalPlayerDeathAndRespawn(dt);
#endif

    // ------------------------------------------------------------------------
    // FrameSnapshot에서 좌표 업데이트
    // ------------------------------------------------------------------------

#ifdef USING_NETWORK
    DequeueNetworkMessage(NetworkMessageType::FrameState);
    if (std::holds_alternative<FrameSnapshot>(m_pendingNetworkMessage.data))
    {
        const FrameSnapshot& receivedSnapshot = std::get<FrameSnapshot>(m_pendingNetworkMessage.data);
		PushNetworkFrameSnapshot(receivedSnapshot);
		const FrameSnapshot& latestSnapshot =
			m_frameSnapshotBuffer.empty() ? receivedSnapshot : m_frameSnapshotBuffer.back();
		FrameSnapshot snapshot = BuildInterpolatedFrameSnapshot(latestSnapshot);

        // Player 좌표 업데이트
        for (const auto& state : snapshot.players)
        {
            // id를 slot으로 사용 (0~3)
            int slot = static_cast<int>(state.id);
            CGameObject* player = GetPlayerBySlot(slot);
            if (!player) continue;


            if (slot == m_localPlayerSlot)
            {
                const XMFLOAT3 currentPos = player->GetPosition();
                const float dx = state.position.x - currentPos.x;
                const float dy = state.position.y - currentPos.y;
                const float dz = state.position.z - currentPos.z;
                const float distSq = dx * dx + dy * dy + dz * dz;

                if (distSq > 4.0f)
                {
                    player->SetPosition(state.position.x, state.position.y, state.position.z);
                }
                else if (distSq > 0.0001f)
                {
                    constexpr float kLocalCorrectionAlpha = 0.35f;
                    player->SetPosition(
                        currentPos.x + dx * kLocalCorrectionAlpha,
                        currentPos.y + dy * kLocalCorrectionAlpha,
                        currentPos.z + dz * kLocalCorrectionAlpha);
                }
            }
            else
            {
                player->SetPosition(state.position.x, state.position.y, state.position.z);
            }

            // yaw 회전 적용
            if (auto* tr = player->GetComponent<CTransformComponent>())
            {
                tr->SetYawDegrees(state.yaw);
            } 

			if ( slot == m_localPlayerSlot )
			{
				if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
					controller->SetYawDegrees(state.yaw);
			}
			
			if ( auto wc = player->GetComponent<CPlayerEquipmentComponent>() )
			{
				wc->SetLoadout(state.weaponType);
			}


			if ( slot == m_localPlayerSlot )
			{
				// 로컬 플레이어로 카메라 동기화
				auto pCamera = GetMainCamera();
				if ( pCamera )
				{
					XMFLOAT3 pos = player->GetPosition();
					pos.y += 1.7f; // 카메라 높이 보정 (플레이어 중심에서 약간 위)
					pCamera->Update(pos, dt);
					pCamera->SetLookAt(pos);
					pCamera->RegenerateViewMatrix();
				}
			}

            // 데모: animation state 강제 적용
            if (auto ac = player->GetAnimController())
            {
                const DecodedAnimStateCode decoded = DecodeStateCode(state.animation.stateCode);
				const auto prevIt = m_prevPlayerNetworkStateCode.find(state.id);
				const uint32_t prevStateCode =
					( prevIt != m_prevPlayerNetworkStateCode.end() ) ? prevIt->second : 0u;
				const DecodedAnimStateCode prevDecoded = DecodeStateCode(prevStateCode);

				if ( prevDecoded.die && !decoded.die )
				{
					ac->ResetToIdleAfterRespawn();

					if ( slot == m_localPlayerSlot )
					{
						m_bLocalPlayerDead = false;
						m_bLocalPlayerRespawnUsed = false;
						m_localPlayerRespawnTimer = 0.0f;

						if ( auto* camera = GetMainCamera() )
						{
							camera->GetYaw() = state.yaw;

							XMFLOAT3 cameraTarget = player->GetPosition();
							cameraTarget.y += 1.7f;
							camera->Update(cameraTarget, 0.0f);
							camera->SetLookAt(cameraTarget);
							camera->RegenerateViewMatrix();
						}
					}
				}

                ac->SetMoveDirection(decoded.hasMove ? decoded.moveDirBits : 0u);
                ac->SetRunRequested(decoded.run);

                if (decoded.die)
                {
					if ( slot == m_localPlayerSlot )
						m_bLocalPlayerDead = true;
					ac->RequestDeath();
                    ac->SetAnimState(EAnimState::Die);
					m_prevPlayerNetworkStateCode[state.id] = state.animation.stateCode;
					continue;
                }

				else if ( decoded.hit )
				{
					if ( !prevDecoded.hit )
					{
						SpawnBloodSplash(player, nullptr, nullptr);
					}

					ac->RequestHit();
					m_prevPlayerNetworkStateCode[state.id] = state.animation.stateCode;
					continue;
				}

				else if ( decoded.roll )
				{
					uint32_t rollDirBits = decoded.hasMove ? decoded.moveDirBits : DIR_FORWARD;

					if (!prevDecoded.roll && slot != m_localPlayerSlot)
					{
						if ( auto* equip = player->GetComponent<CPlayerEquipmentComponent>() )
							equip->RequestRollSfx(rollDirBits);
					}

					ac->RequestRoll(rollDirBits);
					ac->SetAnimState(EAnimState::Attack);

					m_prevPlayerNetworkStateCode[state.id] = state.animation.stateCode;
					continue;
				}
				else if ( decoded.attack )
				{
					if ( !prevDecoded.attack )
					{
						RequestPlayerAttackSfx(player);
					}

					ac->RequestAttack();
					m_prevPlayerNetworkStateCode[state.id] = state.animation.stateCode;
					continue;
				}
                else
                {
                    ac->SetAnimState(decoded.hasMove ? EAnimState::Move : EAnimState::Idle);
                }

				m_prevPlayerNetworkStateCode[state.id] = state.animation.stateCode;
            }
        }


		// Enemy 좌표 업데이트
		// 1) NPC 인덱스 → 오브젝트 매핑 구축
		std::unordered_map<uint64_t, CGameObject*> npcById;
		{
			UINT npcIndex = 0;
			for ( UINT j = 0; j < ( UINT ) m_skinnedObjects.size(); ++j )
			{
				auto* obj = m_skinnedObjects[j].get();
				if ( !obj ) continue;
				auto* tag = obj->GetComponent<CActorTagComponent>();
				if ( !tag || tag->kind != EActorKind::NPC ) continue;
				npcById[npcIndex] = obj;
				++npcIndex;
			}
		}

		// 2) snapshot enemy를 ID 기준으로 적용
		for ( const auto& state : snapshot.enemies )
		{
			auto it = npcById.find(state.id);
			if ( it == npcById.end() ) continue;

			auto* obj = it->second;
			obj->SetPosition(state.position.x, state.position.y, state.position.z);

			if ( auto* tr = obj->GetComponent<CTransformComponent>() )
				tr->SetYawDegrees(state.yaw);

			if ( auto* animComp = obj->GetComponent<CAnimatorComponent>() )
			{
				if ( auto* ctrl = animComp->EnsureMonsterController() )
				{
					const DecodedAnimStateCode decoded = DecodeStateCode(state.animation.stateCode);

					EMonsterAnimState locomotionState = EMonsterAnimState::Idle;
					if ( decoded.hasMove )
						locomotionState = decoded.run ? EMonsterAnimState::Run : EMonsterAnimState::Move;

					ctrl->SetLocomotionState(locomotionState);

					static std::unordered_map<uint64_t, uint32_t> s_prevEnemyStateCode;
					const uint32_t prevStateCode =
						( s_prevEnemyStateCode.find(state.id) != s_prevEnemyStateCode.end() )
						? s_prevEnemyStateCode[state.id]
						: 0u;
					const DecodedAnimStateCode prevDecoded = DecodeStateCode(prevStateCode);

					if ( decoded.die && !prevDecoded.die )
					{
						ctrl->RequestCommand(EMonsterAnimCommand::Death);
					}

					else if ( decoded.hit && !prevDecoded.hit )
					{
						ctrl->RequestCommand(EMonsterAnimCommand::Hit);

						SpawnBloodSplash(obj, nullptr, nullptr);

						if ( auto* hp = obj->GetComponent<CHealthComponent>() )
							hp->RequestHitSfx();
					}

					else if ( decoded.attack && !prevDecoded.attack )
					{
						ctrl->RequestCommand(EMonsterAnimCommand::Attack);
					}

					s_prevEnemyStateCode[state.id] = state.animation.stateCode;
					ctrl->Update(0.0f);
				}
			}
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

		// 사용이 끝난 data는 기본값으로 초기화 (선택적)
		m_pendingNetworkMessage.data = LoadoutData{};
    }
	else
	{
		// 이전 state code를 따라가면서 그때의 애니메이션을 유지
		for ( const auto& [id, stateCode] : m_prevPlayerNetworkStateCode )
		{
			CGameObject* player = GetPlayerBySlot(static_cast< int >( id ));
			if ( !player ) continue;
			if ( auto* ac = player->GetAnimController() )
			{
				const DecodedAnimStateCode decoded = DecodeStateCode(stateCode);
				if ( decoded.die )
					ac->SetAnimState(EAnimState::Die);
				else if ( decoded.hit )
					ac->SetAnimState(EAnimState::Hit);
				else if ( decoded.roll )
					ac->SetAnimState(EAnimState::Attack);
				else if ( decoded.attack )
					ac->SetAnimState(EAnimState::Attack);
				else
					ac->SetAnimState(decoded.hasMove ? EAnimState::Move : EAnimState::Idle);
			}
		}
	}
#endif

	CCamera* camera = GetMainCamera();

#ifndef USING_NETWORK
	const int activeMonsterMegaGridNumber =
		GetLocalPlayerMegaGridNumberForMonsterTick();
#else
	const int activeMonsterMegaGridNumber = -1;
#endif

	for ( UINT j = 0; j < static_cast< UINT >(m_skinnedComponentCache.size()); ++j )
	{
		const SkinnedComponentCache& cache = m_skinnedComponentCache[j];

		CGameObject* obj = cache.object;
		if ( !obj )
			continue;

#ifndef USING_NETWORK
		if ( ShouldSkipMonsterByMegaGrid(obj, j, activeMonsterMegaGridNumber) )
		{
			ResetMonsterToHomeForMegaGridSkip(obj);

			if ( cache.animator )
				cache.animator->SetPoseEvaluationEnabled(false);

			continue;
		}
#endif

#ifdef USING_NETWORK
		const bool shouldEvaluatePose =
			ShouldEvaluateSkinnedPoseThisFrame(j, camera);
#else
		const bool shouldEvaluatePose = true;
#endif

		if ( cache.animator )
			cache.animator->SetPoseEvaluationEnabled(shouldEvaluatePose);

		if ( !obj->GetActive() )
			continue;

		obj->Animate(dt);
	}

	UpdateDynamicGridState();

	UpdatePlayerFootstepSfx();
	UpdateMonsterSfx(dt);

	for ( CGameObject* obj : m_staticGameplayTickObjects )
	{
		if ( !obj )
			continue;

		obj->Animate(dt);
	}

#ifdef USING_NETWORK
	UpdatePlayerBowSfxOnly();
#else
	UpdatePreparedBowArrows();
#endif

	for ( UINT j = 0; j < ( UINT ) m_lightObjects.size(); ++j )
	{
		if ( !m_lightObjects[j] )
			continue;

		m_lightObjects[j]->Animate(dt);
	}
}

void CGameScene::CollisionObjects()
{
	auto IsPlayerMonsterAttackCollisionCandidate =
		[ ](
			const CColliderComponent* a,
			const CColliderComponent* b) -> bool
		{
			if ( !a || !b )
				return false;

			const uint32_t layerA = static_cast< uint32_t >( a->GetLayer() );
			const uint32_t layerB = static_cast< uint32_t >( b->GetLayer() );

			const bool playerWeaponHitsMonster =
				( layerA == kCollisionLayerPlayerWeapon &&
				  layerB == kCollisionLayerMonster ) ||
				( layerB == kCollisionLayerPlayerWeapon &&
				  layerA == kCollisionLayerMonster );

			if ( playerWeaponHitsMonster )
				return true;

			const bool monsterWeaponHitsPlayer =
				( layerA == kCollisionLayerMonsterWeapon &&
				  layerB == kCollisionLayerPlayer ) ||
				( layerB == kCollisionLayerMonsterWeapon &&
				  layerA == kCollisionLayerPlayer );

			if ( monsterWeaponHitsPlayer )
				return true;

			const bool monsterBodyWeaponCapsuleHitsPlayerBody =
				( layerA == kCollisionLayerMonster &&
				  layerB == kCollisionLayerPlayer ) ||
				( layerB == kCollisionLayerMonster &&
				  layerA == kCollisionLayerPlayer );

			if ( monsterBodyWeaponCapsuleHitsPlayerBody )
				return true;

			return false;
		};

	if ( m_bSimulateLocalPlayerMonsterAttackCollision && m_Collision )
	{
		RefreshDynamicCollisionMegaGridMasks();

		m_Collision->OnUpdateFiltered(
			[ this, &IsPlayerMonsterAttackCollisionCandidate ](
				const CColliderComponent* a,
				const CColliderComponent* b) -> bool
			{
				if ( !IsPlayerMonsterAttackCollisionCandidate(a, b) )
					return false;

				return ShouldKeepCollisionPairByMegaGrid(a, b);
			}
		);
	}

	if ( m_bSimulateLocalItemPickup )
	{
		UpdateItemBillboardPickupCollision();
	}

#ifndef USING_NETWORK
	if ( m_bSimulateLocalTeleport )
	{
		TickTowerDoorPortalCooldowns();
		TryTeleportLocalPlayerByTowerDoorPortal();
		TryTeleportLocalPlayerByCastleDoorPortal();
	}
#endif

	UpdateMonsterDeathStates();

#ifndef USING_NETWORK
	UpdateLocalPlayerDeathAndRespawn(0.0f);
#endif
}

void CGameScene::UpdateShaderVariables(ID3D12GraphicsCommandList* /*cmd*/)
{
	PROFILE_RENDER_SCOPE("GameScene::UpdateShaderVariables");
	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	LIGHTS* mappedLights = m_pcbMappedLights[frameIndex];

	if ( mappedLights )
	{
		::ZeroMemory(mappedLights, sizeof(LIGHTS));
		mappedLights->m_xmf4GlobalAmbient = XMFLOAT4(0.20f, 0.20f, 0.20f, 1.0f);

		UINT li = 0;
		for ( auto& obj : m_lightObjects )
		{
			if ( !obj ) continue;

			auto* lc = obj->GetComponent<CLightComponent>();
			if ( !lc ) continue;
			if ( !lc->IsEnabled() ) continue;
			if ( li >= MAX_LIGHTS ) break;

			lc->Fill(mappedLights->m_pLights[li]);
			++li;
		}
	}

	MATERIALS* mappedMaterials = m_pcbMappedMaterials[frameIndex];

	if ( mappedMaterials && m_pMaterials )
		::memcpy(mappedMaterials, m_pMaterials.get(), sizeof(MATERIALS));

	CGameObject* shadowFocus = GetPlayer();
	if ( !shadowFocus )
		shadowFocus = GetPlayerBySlot(0);

	CGameObject* directionalLightObj = nullptr;

	for ( const auto& lightObj : m_lightObjects )
	{
		if ( !lightObj )
			continue;

		auto* lc = lightObj->GetComponent<CLightComponent>();
		if ( !lc )
			continue;

		if ( lc->type == ELightType::Directional )
		{
			directionalLightObj = lightObj.get();
			break;
		}
	}

	m_shadowMap.UpdateData(shadowFocus, directionalLightObj);
	m_shadowMap.UploadConstantBuffer();

	UpdateDepthFogState(m_fElapsedTime);
	m_depthFog.UploadConstantBuffer();

	{
		float hpRatio = 1.0f;

		CGameObject* localPlayer = GetPlayer();
		if ( !localPlayer )
			localPlayer = GetPlayerBySlot(0);

		if ( localPlayer )
		{
			if ( auto* hp = localPlayer->GetComponent<CHealthComponent>() )
				hpRatio = hp->GetHpRatio();
		}

		m_hud.SetHealthRatio(hpRatio);
	}


	if ( m_staticBatch.mappedGameObjects[frameIndex] && !m_staticBatch.objectRefs.empty() )
	{
		const UINT ncb = m_staticBatch.cbElementBytes;

		for ( UINT j = 0; j < static_cast< UINT >(m_staticBatch.objectRefs.size()); ++j )
		{
			auto* obj = m_staticBatch.objectRefs[j];
			if ( !obj ) continue;

			auto* cb =
				reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >(m_staticBatch.mappedGameObjects[frameIndex]) +
					j * ncb
				);

			const XMFLOAT4X4& W = obj->GetWorldMatrix();

			XMStoreFloat4x4(
				&cb->m_xmf4x4World,
				XMMatrixTranspose(XMLoadFloat4x4(&W))
			);

			cb->m_nObjectID = j;
		}
	}

	if ( m_skinnedBatch.mappedGameObjects[frameIndex] && !m_skinnedBatch.objectRefs.empty() )
	{
		const UINT ncb = m_skinnedBatch.cbElementBytes;

		for ( UINT j = 0; j < static_cast< UINT >(m_skinnedBatch.objectRefs.size()); ++j )
		{
			auto* obj = m_skinnedBatch.objectRefs[j];
			if ( !obj ) continue;

			auto* cb =
				reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >(m_skinnedBatch.mappedGameObjects[frameIndex]) +
					j * ncb
				);

			const XMFLOAT4X4& W = obj->GetWorldMatrix();

			XMStoreFloat4x4(
				&cb->m_xmf4x4World,
				XMMatrixTranspose(XMLoadFloat4x4(&W))
			);

			cb->m_nObjectID = j;
		}
	}

	if ( m_colliderBatch.mappedGameObjects[frameIndex] && !m_colliderBatch.objectRefs.empty() )
	{
		const UINT ncb = m_colliderBatch.cbElementBytes;

		for ( UINT j = 0; j < static_cast< UINT >(m_colliderBatch.objectRefs.size()); ++j )
		{
			auto* obj = m_colliderBatch.objectRefs[j];
			if ( !obj ) continue;

			auto* cb =
				reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >(m_colliderBatch.mappedGameObjects[frameIndex]) +
					j * ncb
				);

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
	PROFILE_RENDER_SCOPE("GameScene::OnPrepareRender");

	CScene::OnPrepareRender(cmd, camera);

	UpdateFrameRenderState(camera);
	UpdateShaderVariables(cmd);
	BindFrameRootParameters(cmd);
}

void CGameScene::UpdateFrameRenderState(CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::UpdateFrameRenderState(total)");

	if ( !camera )
		return;

	{
		PROFILE_RENDER_SCOPE("UFRS::UpdateStaticWorldLodSelection");
		UpdateStaticWorldLodSelection(camera);
	}
		BeginStaticOcclusionReadback();
	{
		PROFILE_RENDER_SCOPE("UFRS::UpdateStaticOcclusionCullSelection");
		UpdateStaticOcclusionCullSelection(camera);
	}
	UpdateStaticTreeGridCullSelection(camera);
	{
		PROFILE_RENDER_SCOPE("UFRS::UpdateStaticOcclusionCullSelection");
		BuildStaticVisibleListsForFrame(camera);
	}
	UpdateItemBillboardDistanceCullSelection(camera);
	
	{
		PROFILE_RENDER_SCOPE("UFRS::UpdateSkinnedWorldLodSelection");
		UpdateSkinnedWorldLodSelection(camera);
	}
	BeginSkinnedOcclusionReadback();
	{
		PROFILE_RENDER_SCOPE("UFRS::UpdateSkinnedOcclusionCullSelection");
		UpdateSkinnedOcclusionCullSelection(camera);
	}
}

void CGameScene::BindFrameRootParameters(ID3D12GraphicsCommandList* cmd)
{
	PROFILE_RENDER_SCOPE("GameScene::BindFrameRootParameters");

	if ( !cmd )
		return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	if ( m_pd3dcbLights[frameIndex] )
	{
		cmd->SetGraphicsRootConstantBufferView(
			ROOT_PARAMETER_LIGHT,
			m_pd3dcbLights[frameIndex]->GetGPUVirtualAddress()
		);
	}

	if ( m_pd3dcbMaterials[frameIndex] )
	{
		cmd->SetGraphicsRootConstantBufferView(
			ROOT_PARAMETER_MATERIAL,
			m_pd3dcbMaterials[frameIndex]->GetGPUVirtualAddress()
		);
	}

	m_depthFog.BindConstantBuffer(cmd);
	m_shadowMap.BindConstantBuffer(cmd);
}

void CGameScene::RebindFrameRenderState(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;

	CScene::OnPrepareRender(cmd, camera);
	BindFrameRootParameters(cmd);
}

void CGameScene::RenderSceneGeometry(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::RenderSceneGeometry(total)");

	if ( !m_bStartedGameplayMusic )
	{
		PROFILE_RENDER_SCOPE("GameScene::RenderSceneGeometry::StartGameplayMusic");

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

	if ( m_itemBillboardShader )
	{
		RenderItemBillboards(cmd, camera);
	}

	if ( m_skinnedBatch.shader )
	{
		RenderSkinnedInstanceGroups(cmd, camera);
	}

	{
		PROFILE_RENDER_SCOPE("GameScene::RenderSceneGeometry::StaticOcclusionPass");
		RenderStaticOcclusionPass(cmd, camera);
	}

	{
		PROFILE_RENDER_SCOPE("GameScene::RenderSceneGeometry::SkinnedOcclusionPass");
		RenderSkinnedOcclusionPass(cmd, camera);
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
}

void CGameScene::RenderSceneComposite(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	RenderDepthFog(cmd, camera);

	BindFrameRootParameters(cmd);

	if ( m_transparentItemBillboardShader )
	{
		RenderTransparentItemBillboards(cmd, camera);
	}

	if ( m_swordTrailShader )
	{
		RenderSwordTrails(cmd, camera);
	}

	if ( m_monsterSwordTrailShader )
	{
		RenderMonsterSwordTrails(cmd, camera);
	}

	if ( m_muzzleFlashShader )
	{
		RenderMuzzleFlashes(cmd, camera);
	}

	m_hud.Render(cmd, camera);
}

void CGameScene::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::Render(total)");

	if ( !cmd )
		return;

	{
		PROFILE_RENDER_SCOPE("GameScene::Render::PrepareFrame");
		CScene::OnPrepareRender(cmd, camera);
		UpdateFrameRenderState(camera);
		UpdateShaderVariables(cmd);
		BindFrameRootParameters(cmd);
	}

	{
		PROFILE_RENDER_SCOPE("GameScene::Render::RenderShadowMap");
		RenderShadowMap(cmd);
	}

	{
		PROFILE_RENDER_SCOPE("GameScene::Render::RestoreSceneRenderTargets");
		RestoreSceneRenderTargets(cmd, camera);
	}

	BindFrameRootParameters(cmd);

	{
		PROFILE_RENDER_SCOPE("GameScene::Render::RenderSceneGeometry");
		RenderSceneGeometry(cmd, camera);
	}

	RenderSceneComposite(cmd, camera);
}
