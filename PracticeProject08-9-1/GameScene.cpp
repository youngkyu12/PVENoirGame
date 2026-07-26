//-----------------------------------------------------------------------------
// File: GameScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"
#include "GameSceneHelper.h"

using namespace GameSceneHelper;

namespace
{
	float ComputeSeaBgmBlendForPosition(const XMFLOAT3& position)
	{
		constexpr float kSeaBgmCenterX = 0.0f;
		constexpr float kSeaBgmCenterZ = 400.0f;
		constexpr float kSeaBgmInnerExtent = 520.0f;
		constexpr float kSeaBgmOuterExtent = 600.0f;

		const float dx = std::fabs(position.x - kSeaBgmCenterX);
		const float dz = std::fabs(position.z - kSeaBgmCenterZ);
		const float edgeDistance = std::max(dx, dz);
		const float blendRange = kSeaBgmOuterExtent - kSeaBgmInnerExtent;

		if ( blendRange <= 0.0f )
			return 0.0f;

		return std::clamp(( edgeDistance - kSeaBgmInnerExtent ) / blendRange, 0.0f, 1.0f);
	}
}

CGameScene::CGameScene()
{
	m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };
	m_otherPlayerWorldHpGaugeVisibleForHud.fill(false);
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
	m_networkArrowById.clear();

	m_bulletRefs.clear();
	m_bulletRefs.shrink_to_fit();
	m_networkBulletById.clear();

	m_navMesh.reset();

	m_bSimulateLocalPlayerMonsterAttackCollision = true;
	m_bSimulateLocalAI = true;
	m_bSimulateLocalGhoulAI = true;
	m_bSimulateLocalBowManAI = true;
	m_bSimulateLocalSwordManAI = true;
	m_bSimulateLocalMutantAI = true;
	m_bSimulateLocalBossAI = true;

	m_bPrevDebugDamageMegaGrid5KeyDown = false;

	m_bBossStageBossActivated = false;

	m_bBossSummonSequenceStarted = false;
	m_bBossSummonCircleFadeAgeSec = 0.0f;
	m_pendingBossStageBoss = nullptr;
#ifdef USING_NETWORK
	m_bossSummonVisualCenter = XMFLOAT3(0.0f, 0.0f, 400.0f);
#endif

	m_bBossSummonVisualFadeOutStarted = false;
	m_bBossSummonVisualFadeOutAgeSec = 0.0f;
	m_bBossShockwaveActive = false;
	m_bossShockwaveAgeSec = 0.0f;
	m_bossShockwaveCenter = XMFLOAT3(0.0f, 0.0f, 0.0f);

	m_bBossShockwavePushLocalPlayer = false;
	m_bossShockwavePrevRadius = 0.0f;
	m_bossShockwavePlayerInitialDistance = 0.0f;
	m_bossShockwavePlayerPushDir = XMFLOAT3(0.0f, 0.0f, 1.0f);
	ResetBossPoisonProjectileState();

	m_bBossSummonSequenceStarted = false;
	m_bBossSummonCircleFadeAgeSec = 0.0f;
	m_pendingBossStageBoss = nullptr;
#ifdef USING_NETWORK
	m_bossSummonVisualCenter = XMFLOAT3(0.0f, 0.0f, 400.0f);
#endif

	m_bSimulateLocalMonsterChase = true;
	m_bPrevLocalMonsterChaseToggleKeyDown = false;
	m_bSimulateLocalEnemySpawner = true;
	m_bSimulateLocalPlayerWorldStaticRollback = true;
	m_bSimulateLocalTeleport = true;
	m_bSimulateLocalItemPickup = true;
	m_bCanBossStageDirectly = false;
	m_bSimulateLocalStageTeleport = true;
	m_bPrevLocalStageTeleportKeyDown.fill(false);
	ResetEnemySpawnerTimedGhoulWaveStates();

#ifdef USING_NETWORK
	m_bSimulateLocalPlayerMonsterAttackCollision = false;
	m_bSimulateLocalAI = false;

	m_bSimulateLocalGhoulAI = false;
	m_bSimulateLocalBowManAI = false;
	m_bSimulateLocalSwordManAI = false;
	m_bSimulateLocalMutantAI = false;
	m_bSimulateLocalBossAI = false;

	m_bPrevDebugDamageMegaGrid5KeyDown = false;
	m_bBossStageBossActivated = false;

	m_bBossSummonSequenceStarted = false;
	m_bBossSummonCircleFadeAgeSec = 0.0f;
	m_pendingBossStageBoss = nullptr;
#ifdef USING_NETWORK
	m_bossSummonVisualCenter = XMFLOAT3(0.0f, 0.0f, 400.0f);
#endif

	m_bBossSummonVisualFadeOutStarted = false;
	m_bBossSummonVisualFadeOutAgeSec = 0.0f;
	m_bBossShockwaveActive = false;
	m_bossShockwaveAgeSec = 0.0f;
	m_bossShockwaveCenter = XMFLOAT3(0.0f, 0.0f, 0.0f);

	m_bBossShockwavePushLocalPlayer = false;
	m_bossShockwavePrevRadius = 0.0f;
	m_bossShockwavePlayerInitialDistance = 0.0f;
	m_bossShockwavePlayerPushDir = XMFLOAT3(0.0f, 0.0f, 1.0f);
	ResetBossPoisonProjectileState();

	m_bBossSummonSequenceStarted = false;
	m_bBossSummonCircleFadeAgeSec = 0.0f;
	m_pendingBossStageBoss = nullptr;
#ifdef USING_NETWORK
	m_bossSummonVisualCenter = XMFLOAT3(0.0f, 0.0f, 400.0f);
#endif

	m_bSimulateLocalMonsterChase = false;
	m_bPrevLocalMonsterChaseToggleKeyDown = false;
	m_bSimulateLocalEnemySpawner = false;
	m_bSimulateLocalPlayerWorldStaticRollback = false;
	m_bSimulateLocalTeleport = false;
	m_bSimulateLocalItemPickup = false;
	m_bCanBossStageDirectly = false;
	m_bSimulateLocalStageTeleport = false;
	m_bPrevLocalStageTeleportKeyDown.fill(false);
#endif

	m_bossMeleeSlashCastStates.clear();
	m_bossDeathEffect = BossDeathEffectState{};

	m_bLocalPlayerDead = false;
	m_bLocalPlayerRespawnUsed = false;
	m_localPlayerRespawnTimer = 0.0f;

#ifdef USING_NETWORK
	m_prevPlayerNetworkStateCode.clear();
	m_prevEnemyNetworkStateCode.clear();
	m_prevPlayerAnimTick.clear();
	m_enemyDRStates.clear();
	m_playerDRStates.clear();
	m_projectileDRStates.clear();
	m_networkBossCallIndex = 0;
	m_networkBossCallPendingSummonEffects = 0;
	m_networkBossCallSummonEffectWindowSec = 0.0f;
	m_networkBossCallSummonVisualPreviews.clear();
	m_networkBossCallSummonPreviewKeys.clear();
	m_networkBossCallSummonEffectEnemyIds.clear();
	m_prevNetworkEnemyPositions.clear();
	m_playedSpawnFxKeys.clear();
#endif
	m_playerWeaponDamageTierIndex = 0;
	m_deadMonsters.clear();

	m_bLocalPlayerInsideCastleCenterMegaGrid = false;
	m_bMegaGrid5DirectionalLightProfileActive = false;
	m_bBossStageBgmActive = false;

	m_inventoryItemCounts.fill(0);
	m_bPrevInventoryUseKeyDown.fill(false);

	for ( std::array<float, CGameSceneHUD::kInventorySlotCount>& accumulators : m_inventoryBuffParticleEmitAccumulators )
		accumulators.fill(0.0f);

	m_megaGrid4LowYPoisonStates = {};
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

bool CGameScene::IsAnyVillageWallTreeCullDoorProbeVisible(int megaGridNumber, CCamera* camera) const
{
	if ( !camera )
		return true;

	bool foundProbe = false;

	for ( size_t i = 0; i < m_staticOcclusionEntries.size(); ++i )
	{
		const StaticOcclusionEntry& entry = m_staticOcclusionEntries[i];

		if ( entry.kind != EStaticOcclusionEntryKind::TreeDoorProbe )
			continue;

		if ( entry.treeProbeMegaGridNumber != megaGridNumber )
			continue;

		if ( !entry.enabled )
			continue;

		if ( !entry.hasWorldBounds )
			continue;

		foundProbe = true;

		BoundingOrientedBox testBounds = entry.worldBounds;
		if ( !camera->IsInFrustum(testBounds) )
			continue;

		if ( !m_bStaticOcclusionQueryResultsValid )
			return true;

		if ( i >= m_staticOcclusionQuerySampleCounts.size() )
			return true;

		if ( i >= m_staticOcclusionLastFrameIssuedFlags.size() )
			return true;

		if ( i >= m_staticOcclusionZeroSampleFrameCounts.size() )
			return true;

		if ( m_staticOcclusionLastFrameIssuedFlags[i] == 0 )
			return true;

		if ( m_staticOcclusionQuerySampleCounts[i] > 0ull )
			return true;

		if ( m_staticOcclusionZeroSampleFrameCounts[i] < m_staticOcclusionHideFrameThreshold )
			return true;
	}

	return !foundProbe;
}

bool CGameScene::ShouldCullTreesByVillageDoorProbes(CCamera* camera) const
{
	if ( !m_sceneGrid.IsInitialized() )
		return false;

	if ( !camera )
		return false;

	CGameObject* localPlayer = GetPlayer();

	if ( !localPlayer )
		localPlayer = GetPlayerBySlot(0);

	if ( !localPlayer )
		return false;

	if ( !IsPlayerInsideMegaGridCenter(localPlayer) )
		return false;

	const XMFLOAT3 playerPosition = localPlayer->GetPosition();
	const int megaGridNumber = m_sceneGrid.MegaGridNumberFromWorldPosition(playerPosition.x, playerPosition.z);

	if ( megaGridNumber == 4 )
		return false;

	if ( megaGridNumber == 5 )
		return true;

	if ( playerPosition.y >= kDisableVillageTreeCullPlayerHeight )
		return false;

	return !IsAnyVillageWallTreeCullDoorProbeVisible(megaGridNumber, camera);
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

			float sourceBottomY = 0.0f;
			float targetBottomY = 0.0f;
			float appliedYOffset = 0.0f;

			bool lockPlayerYAfterTeleport = false;

			const bool hasSourceBottom =
				ComputeDoorGroupBottomY(portal, sourceRefs, sourceBottomY);

			const bool hasTargetBottom =
				ComputeDoorGroupBottomY(portal, targetRefs, targetBottomY);

			if ( hasTargetBottom )
			{
				// source 문보다 target 문이 충분히 높을 때만 "타워 위로 올라감"으로 판단한다.
				// 절대 높이 threshold만 쓰면 하단 문 OOBB가 약간 높게 잡힌 경우에도 FixedY가 걸릴 수 있다.
				constexpr float kTowerDoorPortalLevelDeltaEpsilon = 5.0f;

				const bool targetIsUpper =
					hasSourceBottom
					? ( targetBottomY > sourceBottomY + kTowerDoorPortalLevelDeltaEpsilon )
					: ( targetBottomY > kTowerDoorPortalUpperHeightThreshold );

				appliedYOffset =
					targetIsUpper
					? kTowerDoorPortalUpperExitYOffset
					: kTowerDoorPortalLowerExitYOffset;

				dst.y = targetBottomY + appliedYOffset;

				lockPlayerYAfterTeleport = targetIsUpper;
			}
			else
			{
				dst.y = playerPos.y;

				// target 문의 bottom을 못 구한 경우에는 안전하게 terrain attach 모드로 복귀시킨다.
				lockPlayerYAfterTeleport = false;
			}

			if ( m_Collision )
			{
				const XMFLOAT3 originalPos = player->GetPosition();
				XMFLOAT3 resolvedDst = dst;
				bool foundClearDestination = false;

				const int maxResolveSteps =
					static_cast< int >(
						kTowerDoorPortalMaxVerticalResolveDistance /
						kTowerDoorPortalVerticalResolveStep
					);

				for ( int step = 0; step <= maxResolveSteps; ++step )
				{
					player->SetPosition(resolvedDst);
					playerCollider->UpdateWorldBounds();

					if ( !m_Collision->HasCollisionWithWorldStatic(playerCollider) )
					{
						foundClearDestination = true;
						break;
					}

					resolvedDst.y += kTowerDoorPortalVerticalResolveStep;
				}

				player->SetPosition(originalPos);
				playerCollider->UpdateWorldBounds();

				if ( !foundClearDestination )
					return false;

				dst = resolvedDst;
			}

			player->SetPosition(dst);

			if ( auto* terrainAttach = player->GetComponent<CTerrainAttachComponent>() )
			{
				if ( lockPlayerYAfterTeleport )
				{
					terrainAttach->SetFixedY(dst.y);
				}
				else
				{
					terrainAttach->ClearFixedY();

					// 하단으로 내려온 경우에는 다음 LateUpdate까지 기다리지 말고
					// 즉시 terrain attach 상태로 복귀시킨다.
					terrainAttach->SnapToTerrain();

					// SnapToTerrain()이 y를 바꿨을 수 있으므로 dst도 갱신한다.
					dst = player->GetPosition();
				}
			}

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

	if ( pos.y <= kEnemySpawnerInactiveY )
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
	UpdateMegaGrid5DirectionalLightState();
}

void CGameScene::UpdateDynamicGridState()
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	for ( auto& tracker : m_playerGridTrackers )
		RefreshDynamicTracker(tracker, EGridDynamicKind::Player);

	UpdateMegaGridState();
	UpdateCastleCenterMegaGridState();
	UpdateMegaGrid5DirectionalLightState();
}

void CGameScene::UpdateMegaGridState()
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	bool isAnyPlayerInsideMegaGrid5CenterSquare = false;

	for ( const GridDynamicTracker& tracker : m_playerGridTrackers )
	{
		if ( !tracker.occupied )
			continue;

		int megaX = -1;
		int megaZ = -1;

		if ( !m_sceneGrid.FineCellToMegaGridCell(tracker.prevCellX, tracker.prevCellZ, megaX, megaZ) )
			continue;

		if ( megaX == kCastleCenterMegaGridX && megaZ == kCastleCenterMegaGridZ )
		{
			if ( tracker.object )
			{
				const XMFLOAT3 pos = tracker.object->GetPosition();

				if ( IsWorldPositionInsideMegaGrid5CenterSquare250(pos.x, pos.z) )
					isAnyPlayerInsideMegaGrid5CenterSquare = true;
			}

			continue;
		}

		if ( !m_sceneGrid.IsFineCellInsideMegaGridApproachZone(megaX, megaZ, tracker.prevCellX, tracker.prevCellZ) )
			continue;

		if ( !m_sceneGrid.HasMegaGridPlayerApproached(megaX, megaZ) )
			m_sceneGrid.SetMegaGridPlayerApproached(megaX, megaZ, true);
	}

	m_sceneGrid.SetMegaGridPlayerApproached(kCastleCenterMegaGridX, kCastleCenterMegaGridZ, isAnyPlayerInsideMegaGrid5CenterSquare);
}

void CGameScene::UpdateMegaGrid5DirectionalLightState()
{
	if ( !m_sceneGrid.IsInitialized() )
	{
		ApplyMegaGrid5DirectionalLightProfile(false);
		return;
	}

	CGameObject* player = GetPlayer();

	if ( !player )
		player = GetPlayerBySlot(0);

	if ( !player )
	{
		ApplyMegaGrid5DirectionalLightProfile(false);
		return;
	}

	const XMFLOAT3 pos = player->GetPosition();

	const int megaGridNumber =
		m_sceneGrid.MegaGridNumberFromWorldPosition(pos.x, pos.z);

	const bool shouldUseMega5Light =
		( megaGridNumber == 5 );

	ApplyMegaGrid5DirectionalLightProfile(shouldUseMega5Light);
}

void CGameScene::ApplyMegaGrid5DirectionalLightProfile(bool enabled)
{
	if ( m_bMegaGrid5DirectionalLightProfileActive == enabled )
		return;

	CGameObject* directionalLightObject = nullptr;
	CLightComponent* directionalLight = nullptr;

	for ( const auto& lightObject : m_lightObjects )
	{
		if ( !lightObject )
			continue;

		auto* lc = lightObject->GetComponent<CLightComponent>();
		if ( !lc )
			continue;

		if ( lc->type != ELightType::Directional )
			continue;

		directionalLightObject = lightObject.get();
		directionalLight = lc;
		break;
	}

	if ( !directionalLightObject || !directionalLight )
		return;

	if ( auto* tr = directionalLightObject->GetComponent<CTransformComponent>() )
	{
		if ( enabled )
		{
			tr->SetLookDirection(XMFLOAT3(0.0f, -1.0f, 0.0f));
		}
		else
		{
			tr->SetLookDirection(XMFLOAT3(1.0f, -1.0f, 0.3f));
		}
	}

	if ( enabled )
	{
		directionalLight->diffuse =
			XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	}
	else
	{
		directionalLight->diffuse =
			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	m_bMegaGrid5DirectionalLightProfileActive = enabled;

#ifndef USING_NETWORK
	char buf[256];
	sprintf_s(
		buf,
		"[DirectionalLight][Mega5Profile] enabled=%d dir=(%.1f, %.1f, %.1f) diffuse=(%.1f, %.1f, %.1f, %.1f)\n",
		enabled ? 1 : 0,
		enabled ? 0.0f : 1.0f,
		-1.0f,
		enabled ? 0.0f : 0.3f,
		directionalLight->diffuse.x,
		directionalLight->diffuse.y,
		directionalLight->diffuse.z,
		directionalLight->diffuse.w
	);
#endif
}

bool CGameScene::IsPlayerInsideMegaGrid4LowYPoisonArea(const CGameObject* player) const
{
	if ( !player )
		return false;

	const XMFLOAT3 pos = player->GetPosition();

	if ( pos.y > kMegaGrid4LowYPoisonMaxY )
		return false;

	const XMFLOAT3 center = ComputeMegaGridCenterPosition(kMegaGrid4LowYPoisonMegaGridNumber, 0.0f);

	const float dx = std::fabs(pos.x - center.x);
	const float dz = std::fabs(pos.z - center.z);

	return dx <= kMegaGrid4LowYPoisonHalfExtent && dz <= kMegaGrid4LowYPoisonHalfExtent;
}

void CGameScene::UpdateMegaGrid4LowYPoison(float dt)
{
	if ( dt <= 0.0f )
		return;

	for ( int slot = 0; slot < static_cast< int >(m_playersBySlot.size()); ++slot )
	{
		CGameObject* player = m_playersBySlot[static_cast< size_t >(slot)];
		MegaGrid4LowYPoisonState& state = m_megaGrid4LowYPoisonStates[static_cast< size_t >(slot)];

		if ( !player )
		{
			state = MegaGrid4LowYPoisonState{};
			continue;
		}

		CHealthComponent* hp = player->GetComponent<CHealthComponent>();

		if ( !hp || hp->IsDead() )
		{
			state = MegaGrid4LowYPoisonState{};
			continue;
		}

		const bool insidePoisonArea = IsPlayerInsideMegaGrid4LowYPoisonArea(player);

		if ( insidePoisonArea )
		{
			state.exposureSec += dt;

			if ( state.exposureSec >= kMegaGrid4LowYPoisonGraceSec )
			{
				state.exposureSec = kMegaGrid4LowYPoisonGraceSec;
				state.poisoned = true;
			}
		}
		else
		{
			state.exposureSec -= dt;

			if ( state.exposureSec <= 0.0f )
			{
				state = MegaGrid4LowYPoisonState{};
				continue;
			}

			if ( !state.poisoned )
				state.damageAccumulatorSec = 0.0f;
		}

		if ( !state.poisoned )
			continue;

		if ( !insidePoisonArea )
		{
			state.damageAccumulatorSec = 0.0f;
			continue;
		}

		state.damageAccumulatorSec += dt;

		if ( state.damageAccumulatorSec < kMegaGrid4LowYPoisonDamageIntervalSec )
			continue;

		const int tickCount = static_cast< int >(state.damageAccumulatorSec / kMegaGrid4LowYPoisonDamageIntervalSec);
		state.damageAccumulatorSec -= static_cast< float >(tickCount) * kMegaGrid4LowYPoisonDamageIntervalSec;

		const int damage = tickCount * kMegaGrid4LowYPoisonDamagePerTick;
#ifndef USING_NETWORK
		hp->TakeDamage(damage, false);
#else
		UNREFERENCED_PARAMETER(damage);
#endif
	}

	float poisonOverlayAlpha = 0.0f;

	if ( m_localPlayerSlot >= 0 && m_localPlayerSlot < static_cast< int >(m_megaGrid4LowYPoisonStates.size()) )
	{
		const MegaGrid4LowYPoisonState& localState = m_megaGrid4LowYPoisonStates[static_cast< size_t >(m_localPlayerSlot)];

		if ( kMegaGrid4LowYPoisonGraceSec > 0.0f )
			poisonOverlayAlpha = localState.exposureSec / kMegaGrid4LowYPoisonGraceSec;

		if ( poisonOverlayAlpha < 0.0f )
			poisonOverlayAlpha = 0.0f;

		if ( poisonOverlayAlpha > 1.0f )
			poisonOverlayAlpha = 1.0f;
	}

	m_hud.SetPoisonOverlayAlpha(poisonOverlayAlpha);
}

XMFLOAT3 CGameScene::ComputeMegaGridCenterPosition(
	int megaGridNumber,
	float y) const
{
	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return XMFLOAT3(0.0f, y, 0.0f);

	const int zeroBased = megaGridNumber - 1;
	const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
	const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

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

	return XMFLOAT3(centerX, y, centerZ);
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

bool CGameScene::IsMegaGridNumberCleared(int megaGridNumber) const
{
	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return false;

	const int zeroBased = megaGridNumber - 1;
	const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
	const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

	return m_sceneGrid.IsMegaGridCleared(megaX, megaZ);
}

bool CGameScene::ShouldBlockEnemySpawnerByClearedPrerequisite(
	int targetMegaGridNumber,
	int& outBlockerMegaGridNumber) const
{
	outBlockerMegaGridNumber = -1;

	switch ( targetMegaGridNumber )
	{
	case 6:
		if ( IsMegaGridNumberCleared(3) )
		{
			outBlockerMegaGridNumber = 3;
			return true;
		}
		break;

	case 8:
		if ( IsMegaGridNumberCleared(7) )
		{
			outBlockerMegaGridNumber = 7;
			return true;
		}
		break;

	default:
		break;
	}

	return false;
}

int CGameScene::SpawnPreparedEnemiesInMegaGrid(int megaGridNumber)
{
#ifndef USING_NETWORK
	if ( !m_bSimulateLocalEnemySpawner )
		return 0;

	int blockerMegaGridNumber = -1;
	if ( ShouldBlockEnemySpawnerByClearedPrerequisite(megaGridNumber, blockerMegaGridNumber) )
	{
		char buf[256];
		sprintf_s(buf, "[LogicalEnemySpawner] blocked. targetMega=%d blockerMega=%d alreadyCleared=1\n", megaGridNumber, blockerMegaGridNumber);
		return 0;
	}

	const int spawnedCount = SpawnLogicalMegaGrid(megaGridNumber);

	if ( spawnedCount > 0 )
	{
		char buf[256];
		sprintf_s(buf, "[LogicalEnemySpawner] SpawnPreparedEnemiesInMegaGrid mega=%d spawned=%d\n", megaGridNumber, spawnedCount);
	}

	return spawnedCount;
#else
	UNREFERENCED_PARAMETER(megaGridNumber);
	return 0;
#endif
}

int CGameScene::SpawnBossCallMonsters(int callIndex)
{
#ifndef USING_NETWORK
	if ( !m_bSimulateLocalEnemySpawner )
		return 0;

	if ( callIndex < 1 || callIndex > 3 )
		return 0;

	constexpr int megaGridNumber = 5;

	int spawnedTotal = 0;

	if ( m_bossCallSummonPlanCallIndex == callIndex && !m_bossCallSummonPlanEntries.empty() )
	{
		int kindSpawned[4] = { 0, 0, 0, 0 };
		XMFLOAT3 spawnedPosSum = XMFLOAT3(0.0f, 0.0f, 0.0f);
		int spawnedPosCount = 0;

		for ( size_t i = 0; i < m_bossCallSummonPlanEntries.size(); ++i )
		{
			const EnemySpawnerPreviewEntry& preview = m_bossCallSummonPlanEntries[i];
			CGameObject* spawned = SpawnLogicalPreviewEntry(preview);
			const bool success = ( spawned != nullptr );

			if ( success )
			{
				++spawnedTotal;

				const int kindIndex = static_cast< int >(preview.kind);
				if ( kindIndex >= 0 && kindIndex < 4 )
					++kindSpawned[kindIndex];

				const XMFLOAT3 pos = spawned->GetPosition();
				SpawnBossCallSummonWwwEffect(pos, preview.kind);

				spawnedPosSum.x += pos.x;
				spawnedPosSum.y += pos.y;
				spawnedPosSum.z += pos.z;
				++spawnedPosCount;
			}
			else
			{
				SpawnBossCallSummonWwwEffect(preview.spawnPosition, preview.kind);
			}
		}

		if ( spawnedPosCount > 0 )
		{
			const float invCount = 1.0f / static_cast< float >( spawnedPosCount );
			XMFLOAT3 sfxPos{};
			sfxPos.x = spawnedPosSum.x * invCount;
			sfxPos.y = spawnedPosSum.y * invCount;
			sfxPos.z = spawnedPosSum.z * invCount;
			PlayBossCallMonsterSpawnSfxAt(sfxPos);
		}
		else if ( !m_bossCallSummonPlanEntries.empty() )
		{
			PlayBossCallMonsterSpawnSfxAt(m_bossCallSummonPlanEntries.front().spawnPosition);
		}

		m_bossCallSummonPlanCallIndex = -1;
		m_bossCallSummonPlanEntries.clear();

		StartBossCallSummonCircleFadeOut();
		return spawnedTotal;
	}

	auto SpawnKind = [ this, &spawnedTotal ] (EEnemySpawnerEnemyKind kind, int count)
		{
			if ( count <= 0 )
				return;

			std::vector<EnemySpawnerPreviewEntry> previews;
			previews.reserve(static_cast< size_t >( count ));

			PeekLogicalSpawnerEntries(5, kind, count, previews);

			for ( const EnemySpawnerPreviewEntry& preview : previews )
			{
				CGameObject* spawned = SpawnLogicalPreviewEntry(preview);
				if ( !spawned )
					continue;

				++spawnedTotal;

				const XMFLOAT3 pos = spawned->GetPosition();
				SpawnBossCallSummonWwwEffect(pos, preview.kind);
			}
		};

	switch ( callIndex )
	{
	case 1:
		SpawnKind(EEnemySpawnerEnemyKind::Ghoul, 30);
		break;
	case 2:
		SpawnKind(EEnemySpawnerEnemyKind::Ghoul, 20);
		SpawnKind(EEnemySpawnerEnemyKind::BowMan, 5);
		SpawnKind(EEnemySpawnerEnemyKind::SwordMan, 5);
		break;
	case 3:
		SpawnKind(EEnemySpawnerEnemyKind::Ghoul, 20);
		SpawnKind(EEnemySpawnerEnemyKind::BowMan, 5);
		SpawnKind(EEnemySpawnerEnemyKind::SwordMan, 5);
		SpawnKind(EEnemySpawnerEnemyKind::Mutant, 5);
		break;
	default:
		break;
	}

	if ( spawnedTotal > 0 )
	{
		XMFLOAT3 sfxPos = AlignPositionYToTerrainGround(XMFLOAT3(400.0f, 0.0f, 400.0f), 0.0f);
		PlayBossCallMonsterSpawnSfxAt(sfxPos);
	}

	StartBossCallSummonCircleFadeOut();
	return spawnedTotal;
#else
	UNREFERENCED_PARAMETER(callIndex);
	return 0;
#endif
}

int CGameScene::TryRunEnemySpawnerEventForMegaGrid(int megaGridNumber)
{
#ifndef USING_NETWORK
	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return 0;

	const int zeroBased = megaGridNumber - 1;
	const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
	const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

	if ( m_sceneGrid.HasMegaGridEventOccurred(megaX, megaZ) )
		return 0;

	int blockerMegaGridNumber = -1;
	if ( ShouldBlockEnemySpawnerByClearedPrerequisite(
		megaGridNumber,
		blockerMegaGridNumber) )
	{
		m_sceneGrid.SetMegaGridEventOccurred(megaX, megaZ, true);
		return 0;
	}

	if ( megaGridNumber == 6 || megaGridNumber == 8 )
	{
		const bool started =
			BeginEnemySpawnerTimedGhoulWave(megaGridNumber);

		if ( started )
		{
			m_sceneGrid.SetMegaGridEventOccurred(megaX, megaZ, true);
			return 1;
		}

		return 0;
	}

	// 5번 메가그리드 스포너 풀은 보스 Call 전용으로 사용한다.
	// 접근 이벤트에서 미리 SpawnMegaGrid(5)를 호출하면
	// Call용 풀을 전부 소모하므로 여기서는 아무 것도 하지 않는다.
	if ( megaGridNumber == 5 )
	{
		// 5번 메가그리드 스포너 풀은 보스 Call 전용으로 사용한다.
		// 접근 이벤트에서 SpawnMegaGrid(5)를 호출하면 Call용 풀을 전부 소모한다.
		return 0;
	}

	if ( megaGridNumber == 6 || megaGridNumber == 8 )
	{
		const bool started =
			BeginEnemySpawnerTimedGhoulWave(megaGridNumber);

		if ( started )
		{
			m_sceneGrid.SetMegaGridEventOccurred(megaX, megaZ, true);
			return 1;
		}

		return 0;
	}

	const int spawnedCount = SpawnPreparedEnemiesInMegaGrid(megaGridNumber);

	if ( spawnedCount > 0 )
	{
		m_sceneGrid.SetMegaGridEventOccurred(megaX, megaZ, true);
	}

	return spawnedCount;
#else
	UNREFERENCED_PARAMETER(megaGridNumber);
	return 0;
#endif
}

bool CGameScene::BeginEnemySpawnerTimedGhoulWave(int megaGridNumber)
{
#ifndef USING_NETWORK
	if ( !m_bSimulateLocalEnemySpawner )
		return false;

	if ( megaGridNumber != 6 && megaGridNumber != 8 )
		return false;

	if ( megaGridNumber < 1 ||
		 megaGridNumber > CSceneGrid::kMegaGridCount )
	{
		return false;
	}

	EnemySpawnerTimedGhoulWaveState& state =
		m_enemySpawnerTimedGhoulWaves[( size_t ) megaGridNumber];

	if ( state.active )
		return false;

	state = EnemySpawnerTimedGhoulWaveState{};
	state.active = true;
	state.nextBatchIndex = 0;
	state.accumulatorSec = 0.0f;

	// 가동 즉시 0번 batch 생성.
	const int spawnedNow =
		SpawnEnemySpawnerDoorGhoulBatch(
			megaGridNumber,
			state.nextBatchIndex
		);

	if ( spawnedNow <= 0 )
	{
		state = EnemySpawnerTimedGhoulWaveState{};
		return false;
	}

	const int sirenMegaGridNumber =
		( megaGridNumber == 6 )
		? 3
		: 7;

	const XMFLOAT3 sirenPosition =
		ComputeMegaGridCenterPosition(
			sirenMegaGridNumber,
			50.0f
		);

	PlayEnemySpawnerSirenSfxAt(sirenPosition);

	++state.nextBatchIndex;

	if ( state.nextBatchIndex >= kEnemySpawnerDoorBatchCount )
		state.active = false;

	char buf[256];
	sprintf_s(
		buf,
		"[EnemySpawnerWave] started. mega=%d firstBatchSpawned=%d\n",
		megaGridNumber,
		spawnedNow
	);

	return true;
#else
	UNREFERENCED_PARAMETER(megaGridNumber);
	return false;
#endif
}

void CGameScene::UpdateEnemySpawnerTimedGhoulWaves(float dt)
{
#ifndef USING_NETWORK
	if ( !m_bSimulateLocalEnemySpawner )
		return;

	if ( dt < 0.0f )
		dt = 0.0f;

	const int targetMegaGrids[2] = { 6, 8 };

	for ( int megaGridNumber : targetMegaGrids )
	{
		EnemySpawnerTimedGhoulWaveState& state =
			m_enemySpawnerTimedGhoulWaves[( size_t ) megaGridNumber];

		if ( !state.active )
			continue;

		state.accumulatorSec += dt;

		while ( state.active &&
				state.accumulatorSec >= kEnemySpawnerDoorBatchIntervalSec )
		{
			state.accumulatorSec -= kEnemySpawnerDoorBatchIntervalSec;

			if ( state.nextBatchIndex >= kEnemySpawnerDoorBatchCount )
			{
				state.active = false;
				break;
			}

			SpawnEnemySpawnerDoorGhoulBatch(
				megaGridNumber,
				state.nextBatchIndex
			);

			++state.nextBatchIndex;

			if ( state.nextBatchIndex >= kEnemySpawnerDoorBatchCount )
			{
				state.active = false;

				char buf[256];
				sprintf_s(
					buf,
					"[EnemySpawnerWave] finished. mega=%d\n",
					megaGridNumber
				);
			}
		}
	}
#else
	UNREFERENCED_PARAMETER(dt);
#endif
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

int CGameScene::SpawnEnemySpawnerDoorGhoulBatch(int megaGridNumber, int batchIndex)
{
#ifndef USING_NETWORK
	if ( !m_bSimulateLocalEnemySpawner )
		return 0;

	if ( megaGridNumber != 6 && megaGridNumber != 8 )
		return 0;

	if ( batchIndex < 0 || batchIndex >= kEnemySpawnerDoorBatchCount )
		return 0;

	int spawnedCount = 0;

	for ( int wallIndex = 0; wallIndex < kEnemySpawnerDoorWallCount; ++wallIndex )
	{
		const float yawDeg = ComputeEnemySpawnerDoorGhoulSpawnYawDeg(wallIndex);

		for ( int slotIndex = 0; slotIndex < kEnemySpawnerDoorSlotsPerWall; ++slotIndex )
		{
			const XMFLOAT3 pos = ComputeEnemySpawnerDoorGhoulSpawnPosition(megaGridNumber, wallIndex, slotIndex);

			CGameObject* ghoul = SpawnLogicalEnemyAt(megaGridNumber, EEnemySpawnerEnemyKind::Ghoul, pos, yawDeg);
			if ( !ghoul )
				continue;

			if ( auto* ai = ghoul->GetComponent<CEnemySpawnerGhoulAIComponent>() )
			{
				ai->SetHomeTransform(pos, yawDeg);
				ai->ConfigureSpawnerGhoulAI(megaGridNumber, 60.0f);
			}

			++spawnedCount;
		}
	}

	char buf[256];
	sprintf_s(buf, "[LogicalEnemySpawnerWave] mega=%d batch=%d spawned=%d\n", megaGridNumber, batchIndex, spawnedCount);

	return spawnedCount;
#else
	UNREFERENCED_PARAMETER(megaGridNumber);
	UNREFERENCED_PARAMETER(batchIndex);
	return 0;
#endif
}

XMFLOAT3 CGameScene::ComputeEnemySpawnerDoorGhoulSpawnPosition(
	int megaGridNumber,
	int wallIndex,
	int slotIndex) const
{
	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return XMFLOAT3(0.0f, 0.0f, 0.0f);

	if ( wallIndex < 0 )
		wallIndex = 0;

	if ( wallIndex >= kEnemySpawnerDoorWallCount )
		wallIndex = kEnemySpawnerDoorWallCount - 1;

	if ( slotIndex < 0 )
		slotIndex = 0;

	if ( slotIndex >= kEnemySpawnerDoorSlotsPerWall )
		slotIndex = kEnemySpawnerDoorSlotsPerWall - 1;

	const int zeroBased = megaGridNumber - 1;
	const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
	const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

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

	const float offset =
		( static_cast< float >( slotIndex ) -
		  static_cast< float >( kEnemySpawnerDoorSlotsPerWall - 1 ) * 0.5f )
		* kEnemySpawnerDoorSlotSpacing;

	XMFLOAT3 pos(0.0f, 0.0f, 0.0f);

	switch ( wallIndex )
	{
	case 0:
		pos.x = centerX - kEnemySpawnerDoorWallHalfExtent;
		pos.z = centerZ + offset;
		break;

	case 1:
		pos.x = centerX + kEnemySpawnerDoorWallHalfExtent;
		pos.z = centerZ + offset;
		break;

	case 2:
		pos.x = centerX + offset;
		pos.z = centerZ - kEnemySpawnerDoorWallHalfExtent;
		break;

	case 3:
	default:
		pos.x = centerX + offset;
		pos.z = centerZ + kEnemySpawnerDoorWallHalfExtent;
		break;
	}

	pos.y = 0.0f;
	return pos;
}

float CGameScene::ComputeEnemySpawnerDoorGhoulSpawnYawDeg(
	int wallIndex) const
{
	switch ( wallIndex )
	{
	case 0:
		// left wall -> center, +X
		return 90.0f;

	case 1:
		// right wall -> center, -X
		return -90.0f;

	case 2:
		// bottom wall -> center, +Z
		return 0.0f;

	case 3:
	default:
		// top wall -> center, -Z
		return 180.0f;
	}
}

void CGameScene::ResetEnemySpawnerTimedGhoulWaveStates()
{
	for ( EnemySpawnerTimedGhoulWaveState& state : m_enemySpawnerTimedGhoulWaves )
	{
		state = EnemySpawnerTimedGhoulWaveState{};
	}
}

std::vector<CGameObject*>* CGameScene::GetLogicalMonsterVisualPool(ELogicalMonsterKind kind)
{
	switch ( kind )
	{
	case ELogicalMonsterKind::Ghoul:
		return &m_ghoulRefs;
	case ELogicalMonsterKind::SwordMan:
		return &m_swordManRefs;
	case ELogicalMonsterKind::BowMan:
		return &m_bowManRefs;
	case ELogicalMonsterKind::Mutant:
		return &m_MutantRefs;
	case ELogicalMonsterKind::Boss:
		return &m_bossRefs;
	default:
		return nullptr;
	}
}

const std::vector<CGameObject*>* CGameScene::GetLogicalMonsterVisualPool(ELogicalMonsterKind kind) const
{
	switch ( kind )
	{
	case ELogicalMonsterKind::Ghoul:
		return &m_ghoulRefs;
	case ELogicalMonsterKind::SwordMan:
		return &m_swordManRefs;
	case ELogicalMonsterKind::BowMan:
		return &m_bowManRefs;
	case ELogicalMonsterKind::Mutant:
		return &m_MutantRefs;
	case ELogicalMonsterKind::Boss:
		return &m_bossRefs;
	default:
		return nullptr;
	}
}

CGameObject* CGameScene::AcquireFreeLogicalMonsterVisual(ELogicalMonsterKind kind) const
{
	const std::vector<CGameObject*>* pool = GetLogicalMonsterVisualPool(kind);
	if ( !pool )
		return nullptr;

	for ( CGameObject* object : *pool )
	{
		if ( !object )
			continue;

		if ( FindLogicalMonsterIndexByObject(object) >= 0 )
			continue;

		return object;
	}

	return nullptr;
}

void CGameScene::BuildWantedLogicalMonsterSet(std::vector<int>& outWantedLogicalIndices) const
{
	outWantedLogicalIndices.clear();

	const int activeMegaGridNumber = GetLocalPlayerMegaGridNumberForMonsterTick();
	if ( activeMegaGridNumber < 1 || activeMegaGridNumber > CSceneGrid::kMegaGridCount )
		return;

	CGameObject* player = GetPlayer();
	if ( !player )
		player = GetPlayerBySlot(0);

	const XMFLOAT3 playerPos = player ? player->GetPosition() : XMFLOAT3(0.0f, 0.0f, 0.0f);

	std::vector<int> candidates;

	const std::vector<int>& logicalIndices = m_logicalMonsterIndicesByMegaGrid[static_cast< size_t >( activeMegaGridNumber )];
	candidates.reserve(logicalIndices.size());

	for ( int logicalIndex : logicalIndices )
	{
		if ( logicalIndex < 0 || logicalIndex >= static_cast< int >(m_logicalMonsters.size()) )
			continue;

		const LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

		if ( !logical.active )
			continue;

		if ( logical.kind == ELogicalMonsterKind::Boss )
			continue;

		candidates.push_back(logicalIndex);
	}

	std::stable_sort(candidates.begin(), candidates.end(), [ this, playerPos ] (int a, int b)
	{
		const LogicalMonsterState& lhs = m_logicalMonsters[static_cast< size_t >( a )];
		const LogicalMonsterState& rhs = m_logicalMonsters[static_cast< size_t >( b )];

		auto GetPriority = [ ] (const LogicalMonsterState& logical) -> int
			{
				if ( logical.keyTrigger )
					return 0;

				if ( logical.spawnerEntry && logical.active && logical.spawnerConsumed )
					return 1;

				if ( logical.dead || logical.hp <= 0 )
					return 3;

				return 2;
			};

	const int lhsPriority = GetPriority(lhs);
	const int rhsPriority = GetPriority(rhs);

	if ( lhsPriority != rhsPriority )
		return lhsPriority < rhsPriority;

	const float ldx = lhs.position.x - playerPos.x;
	const float ldz = lhs.position.z - playerPos.z;
	const float rdx = rhs.position.x - playerPos.x;
	const float rdz = rhs.position.z - playerPos.z;

	const float lhsDistSq = ldx * ldx + ldz * ldz;
	const float rhsDistSq = rdx * rdx + rdz * rdz;

	return lhsDistSq < rhsDistSq;
});

	UINT ghoulCount = 0;
	UINT swordManCount = 0;
	UINT bowManCount = 0;
	UINT mutantCount = 0;

	for ( int logicalIndex : candidates )
	{
		const LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

		switch ( logical.kind )
		{
		case ELogicalMonsterKind::Ghoul:
			if ( ghoulCount >= static_cast< UINT >( m_ghoulRefs.size() ) )
				continue;
			++ghoulCount;
			break;
		case ELogicalMonsterKind::SwordMan:
			if ( swordManCount >= static_cast< UINT >( m_swordManRefs.size() ) )
				continue;
			++swordManCount;
			break;
		case ELogicalMonsterKind::BowMan:
			if ( bowManCount >= static_cast< UINT >( m_bowManRefs.size() ) )
				continue;
			++bowManCount;
			break;
		case ELogicalMonsterKind::Mutant:
			if ( mutantCount >= static_cast< UINT >( m_MutantRefs.size() ) )
				continue;
			++mutantCount;
			break;
		default:
			continue;
		}

		outWantedLogicalIndices.push_back(logicalIndex);
	}
}

void CGameScene::ReconcileLogicalMonsterVisualBindings()
{
	std::vector<CGameObject*> currentlyBoundObjects;
	currentlyBoundObjects.reserve(m_logicalMonsterIndexByObject.size());

	for ( const auto& kv : m_logicalMonsterIndexByObject )
	{
		if ( kv.first )
			currentlyBoundObjects.push_back(kv.first);
	}

#ifndef USING_NETWORK
	for ( CGameObject* object : currentlyBoundObjects )
	{
		if ( !object )
			continue;

		if ( IsBossMonsterObject(object) )
			continue;

		if ( object->GetActive() )
			SyncLogicalMonsterFromActualObject(object);
	}
#endif

	std::vector<int> wantedLogicalIndices;
	BuildWantedLogicalMonsterSet(wantedLogicalIndices);

	std::unordered_set<int> wantedSet;
	wantedSet.reserve(wantedLogicalIndices.size());

	for ( int logicalIndex : wantedLogicalIndices )
		wantedSet.insert(logicalIndex);

	for ( int logicalIndex = 0; logicalIndex < static_cast< int >(m_logicalMonsters.size()); ++logicalIndex )
	{
		LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

		if ( logical.kind == ELogicalMonsterKind::Boss )
			continue;

		if ( !logical.boundObject )
			continue;

		if ( wantedSet.find(logicalIndex) != wantedSet.end() )
			continue;

		UnbindActualMonsterFromLogical(logical.boundObject);
	}

	for ( int logicalIndex : wantedLogicalIndices )
	{
		if ( logicalIndex < 0 || logicalIndex >= static_cast< int >(m_logicalMonsters.size()) )
			continue;

		LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

		if ( logical.boundObject )
		{
			SyncActualMonsterFromLogicalState(logical.boundObject, logicalIndex, false);
			continue;
		}

		CGameObject* freeObject = AcquireFreeLogicalMonsterVisual(logical.kind);
		if ( !freeObject )
			continue;

		BindLogicalMonsterToActualObject(logicalIndex, freeObject);
	}

	RebuildSceneGridMonsterRefsFromLogicalBindings();
}

void CGameScene::BindLogicalMonsterToActualObject(int logicalMonsterIndex, CGameObject* monster)
{
	if ( !monster )
		return;

	if ( logicalMonsterIndex < 0 || logicalMonsterIndex >= static_cast< int >(m_logicalMonsters.size()) )
		return;

	LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalMonsterIndex)];

	if ( logical.kind == ELogicalMonsterKind::Boss )
		return;

	const int oldLogicalIndex = FindLogicalMonsterIndexByObject(monster);
	if ( oldLogicalIndex >= 0 && oldLogicalIndex != logicalMonsterIndex )
		UnbindActualMonsterFromLogical(monster);

	if ( logical.boundObject && logical.boundObject != monster )
		UnbindActualMonsterFromLogical(logical.boundObject);

	UINT skinnedBatchObjectIndex = UINT_MAX;
	if ( !FindSkinnedBatchObjectIndex(monster, skinnedBatchObjectIndex) )
		return;

	LinkActualMonsterToLogical(monster, skinnedBatchObjectIndex, logicalMonsterIndex);
	SyncActualMonsterFromLogicalState(monster, logicalMonsterIndex, true);
	ConfigureLogicalSpawnerVisualRuntime(monster, logicalMonsterIndex);
	UpdateActualMonsterMegaGridBinding(monster, skinnedBatchObjectIndex, logical.megaGridNumber);
}

void CGameScene::UnbindActualMonsterFromLogical(CGameObject* monster)
{
	if ( !monster )
		return;

	const int logicalIndex = FindLogicalMonsterIndexByObject(monster);

	if ( logicalIndex >= 0 && logicalIndex < static_cast< int >(m_logicalMonsters.size()) )
	{
#ifndef USING_NETWORK
		if ( monster->GetActive() )
			SyncLogicalMonsterFromActualObject(monster);
#endif

		LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];
		logical.boundObject = nullptr;
		logical.boundSkinnedBatchObjectIndex = UINT_MAX;
	}

	m_logicalMonsterIndexByObject.erase(monster);
	m_deadMonsters.erase(monster);

	CancelMonsterPreparedActions(monster);
	DisableAllMonsterAIComponents(monster);

	if ( auto* renderer = monster->GetComponent<CSkinnedMeshRendererComponent>() )
		renderer->SetEnabled(false);

	if ( auto* collider = monster->GetComponent<CColliderComponent>() )
	{
		collider->SetEnabled(false);
		collider->SetCollisionEnabled(false);
		collider->UpdateWorldBounds();
	}

	if ( auto* weaponHitbox = monster->GetComponent<CMonsterWeaponHitboxComponent>() )
		weaponHitbox->SetEnabled(false);

	XMFLOAT3 inactivePosition = monster->GetPosition();
	inactivePosition.y = kEnemySpawnerInactiveY;
	monster->SetPosition(inactivePosition);
	monster->SetActive(false);

	UINT skinnedBatchObjectIndex = UINT_MAX;
	if ( FindSkinnedBatchObjectIndex(monster, skinnedBatchObjectIndex) )
		UpdateActualMonsterMegaGridBinding(monster, skinnedBatchObjectIndex, -1);
}

void CGameScene::SyncActualMonsterFromLogicalState(CGameObject* monster, int logicalMonsterIndex, bool resetRuntimeState)
{
	if ( !monster )
		return;

	if ( logicalMonsterIndex < 0 || logicalMonsterIndex >= static_cast< int >(m_logicalMonsters.size()) )
		return;

	LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalMonsterIndex)];

	const bool logicalDead = logical.dead || logical.hp <= 0;
	const bool shouldActivate = logical.active;
	const bool wasRuntimeDead = m_deadMonsters.find(monster) != m_deadMonsters.end();

	XMFLOAT3 position = logical.position;

#ifndef USING_NETWORK
	position.y = GetTerrainGroundYOrFallback(position.x, position.z, position.y);
#endif

	monster->SetPosition(position);

	if ( auto* tr = monster->GetComponent<CTransformComponent>() )
		tr->SetYawDegrees(logical.yawDeg);

	monster->SetActive(shouldActivate);

	if ( auto* renderer = monster->GetComponent<CSkinnedMeshRendererComponent>() )
		renderer->SetEnabled(shouldActivate);

	if ( auto* hp = monster->GetComponent<CHealthComponent>() )
	{
		if ( logicalDead )
		{
			hp->SetCurrentHp(0);
		}
		else
		{
			hp->ResetToMax();
			hp->SetCurrentHp(std::clamp(logical.hp, 1, std::max(1, logical.maxHp)));
		}
	}

	if ( auto* collider = monster->GetComponent<CColliderComponent>() )
	{
		collider->CancelDeferredDisable();

		const bool collisionEnabled = shouldActivate && !logicalDead;
		collider->SetEnabled(collisionEnabled);
		collider->SetCollisionEnabled(collisionEnabled);
		collider->UpdateWorldBounds();
	}

	if ( logicalDead )
	{
		const bool shouldApplyDeathVisualState = resetRuntimeState || !wasRuntimeDead;

		m_deadMonsters.insert(monster);

		if ( shouldApplyDeathVisualState )
		{
			CancelMonsterPreparedActions(monster);
			DisableAllMonsterAIComponents(monster);

			if ( auto* weaponHitbox = monster->GetComponent<CMonsterWeaponHitboxComponent>() )
				weaponHitbox->SetEnabled(false);

			if ( auto* animComp = monster->GetComponent<CAnimatorComponent>() )
			{
				if ( auto* ctrl = animComp->EnsureMonsterController() )
				{
					if ( resetRuntimeState )
						ctrl->PlayDeathFinalPose();
					else
						ctrl->PlayDeathFromStart();
				}
			}
		}
	}
	else
	{
		const bool shouldResetAliveRuntime = resetRuntimeState || wasRuntimeDead;

		m_deadMonsters.erase(monster);

		if ( shouldResetAliveRuntime )
		{
			CancelMonsterPreparedActions(monster);

			if ( auto* weaponHitbox = monster->GetComponent<CMonsterWeaponHitboxComponent>() )
				weaponHitbox->SetEnabled(false);

			if ( auto* animComp = monster->GetComponent<CAnimatorComponent>() )
			{
				if ( auto* ctrl = animComp->EnsureMonsterController() )
					ctrl->ResetRuntimeState(EMonsterAnimState::Idle);
			}
		}
	}

	logical.position = position;
	logical.active = shouldActivate;
}

void CGameScene::UpdateActualMonsterMegaGridBinding(CGameObject* monster, UINT skinnedBatchObjectIndex, int megaGridNumber)
{
	if ( !monster )
		return;

	if ( skinnedBatchObjectIndex >= static_cast< UINT >( m_skinnedMonsterMegaGridNumbers.size() ) )
		m_skinnedMonsterMegaGridNumbers.resize(static_cast< size_t >( skinnedBatchObjectIndex ) + 1, -1);

	m_skinnedMonsterMegaGridNumbers[static_cast< size_t >( skinnedBatchObjectIndex )] = megaGridNumber;

	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
	{
		SetObjectCollisionMegaGridMask(monster, 0, true);
		return;
	}

	const uint16_t monsterMegaGridMask = static_cast< uint16_t >( 1u << ( megaGridNumber - 1 ) );
	SetObjectCollisionMegaGridMask(monster, monsterMegaGridMask, true);
}

void CGameScene::RebuildSceneGridMonsterRefsFromLogicalBindings()
{
	m_sceneGrid.ClearMegaGridMonsters();

	for ( const LogicalMonsterState& logical : m_logicalMonsters )
	{
		if ( !logical.boundObject )
			continue;

		if ( !logical.active )
			continue;

		if ( !logical.boundObject->GetActive() )
			continue;

		if ( logical.megaGridNumber < 1 || logical.megaGridNumber > CSceneGrid::kMegaGridCount )
			continue;

		const int zeroBased = logical.megaGridNumber - 1;
		const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
		const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

		m_sceneGrid.AddMonsterToMegaGrid(megaX, megaZ, logical.boundObject);
	}
}

void CGameScene::UpdateLogicalMonsterMegaGridIndex(int logicalMonsterIndex, int oldMegaGridNumber)
{
	if ( logicalMonsterIndex < 0 || logicalMonsterIndex >= static_cast< int >(m_logicalMonsters.size()) )
		return;

	const LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalMonsterIndex)];

	if ( oldMegaGridNumber == logical.megaGridNumber )
		return;

	if ( oldMegaGridNumber >= 1 && oldMegaGridNumber <= CSceneGrid::kMegaGridCount )
	{
		std::vector<int>& oldList = m_logicalMonsterIndicesByMegaGrid[static_cast< size_t >(oldMegaGridNumber)];
		oldList.erase(std::remove(oldList.begin(), oldList.end(), logicalMonsterIndex), oldList.end());
	}

	if ( logical.megaGridNumber >= 1 && logical.megaGridNumber <= CSceneGrid::kMegaGridCount )
	{
		std::vector<int>& newList = m_logicalMonsterIndicesByMegaGrid[static_cast< size_t >( logical.megaGridNumber )];
		if ( std::find(newList.begin(), newList.end(), logicalMonsterIndex) == newList.end() )
			newList.push_back(logicalMonsterIndex);
	}
}

ELogicalMonsterKind CGameScene::ConvertEnemySpawnerKindToLogicalKind(EEnemySpawnerEnemyKind kind) const
{
	switch ( kind )
	{
	case EEnemySpawnerEnemyKind::SwordMan:
		return ELogicalMonsterKind::SwordMan;
	case EEnemySpawnerEnemyKind::BowMan:
		return ELogicalMonsterKind::BowMan;
	case EEnemySpawnerEnemyKind::Mutant:
		return ELogicalMonsterKind::Mutant;
	case EEnemySpawnerEnemyKind::Ghoul:
	default:
		return ELogicalMonsterKind::Ghoul;
	}
}

int CGameScene::FindFreeLogicalSpawnerMonster(int megaGridNumber, EEnemySpawnerEnemyKind kind) const
{
	const ELogicalMonsterKind logicalKind = ConvertEnemySpawnerKindToLogicalKind(kind);

	for ( int logicalIndex = 0; logicalIndex < static_cast< int >(m_logicalMonsters.size()); ++logicalIndex )
	{
		const LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

		if ( !logical.spawnerEntry )
			continue;

		if ( logical.spawnerConsumed )
			continue;

		if ( logical.active )
			continue;

		if ( logical.dead )
			continue;

		if ( logical.kind != logicalKind )
			continue;

		if ( logical.megaGridNumber != megaGridNumber )
			continue;

		return logicalIndex;
	}

	return -1;
}

int CGameScene::PeekLogicalSpawnerEntries(int megaGridNumber, EEnemySpawnerEnemyKind kind, int count, std::vector<EnemySpawnerPreviewEntry>& outEntries) const
{
	if ( count <= 0 )
		return 0;

	const ELogicalMonsterKind logicalKind = ConvertEnemySpawnerKindToLogicalKind(kind);
	int foundCount = 0;

	for ( int logicalIndex = 0; logicalIndex < static_cast< int >(m_logicalMonsters.size()) && foundCount < count; ++logicalIndex )
	{
		const LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

		if ( !logical.spawnerEntry )
			continue;

		if ( logical.spawnerConsumed )
			continue;

		if ( logical.active )
			continue;

		if ( logical.dead )
			continue;

		if ( logical.kind != logicalKind )
			continue;

		if ( logical.megaGridNumber != megaGridNumber )
			continue;

		EnemySpawnerPreviewEntry preview{};
		preview.entryIndex = static_cast< size_t >( logicalIndex );
		preview.logicalMonsterIndex = logicalIndex;
		preview.object = logical.boundObject;
		preview.kind = kind;
		preview.megaGridNumber = megaGridNumber;
		preview.spawnPosition = logical.position;
		preview.yawDeg = logical.yawDeg;

		outEntries.push_back(preview);
		++foundCount;
	}

	return foundCount;
}

CGameObject* CGameScene::ActivateLogicalSpawnerMonster(int logicalMonsterIndex, const XMFLOAT3* overridePosition, const float* overrideYawDeg)
{
#ifndef USING_NETWORK
	if ( logicalMonsterIndex < 0 || logicalMonsterIndex >= static_cast< int >(m_logicalMonsters.size()) )
		return nullptr;

	LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalMonsterIndex)];

	if ( !logical.spawnerEntry )
		return nullptr;

	if ( logical.spawnerConsumed )
		return nullptr;

	if ( logical.dead )
		return nullptr;

	if ( overridePosition )
	{
		const int oldMegaGridNumber = logical.megaGridNumber;

		logical.position = *overridePosition;
		logical.homePosition = *overridePosition;
		logical.megaGridNumber = m_sceneGrid.MegaGridNumberFromWorldPosition(logical.position.x, logical.position.z);

		UpdateLogicalMonsterMegaGridIndex(logicalMonsterIndex, oldMegaGridNumber);
	}

	if ( overrideYawDeg )
		logical.yawDeg = *overrideYawDeg;

	logical.hp = logical.maxHp;
	logical.active = true;
	logical.dead = false;
	logical.spawnerConsumed = true;

	ReconcileLogicalMonsterVisualBindings();

	CGameObject* spawnedObject = logical.boundObject;
	if ( spawnedObject )
		ConfigureLogicalSpawnerVisualRuntime(spawnedObject, logicalMonsterIndex);

	return spawnedObject;
#else
	UNREFERENCED_PARAMETER(logicalMonsterIndex);
	UNREFERENCED_PARAMETER(overridePosition);
	UNREFERENCED_PARAMETER(overrideYawDeg);
	return nullptr;
#endif
}

CGameObject* CGameScene::SpawnLogicalEnemyAt(int megaGridNumber, EEnemySpawnerEnemyKind kind, const XMFLOAT3& position, float yawDeg)
{
#ifndef USING_NETWORK
	const int logicalIndex = FindFreeLogicalSpawnerMonster(megaGridNumber, kind);
	if ( logicalIndex < 0 )
		return nullptr;

	return ActivateLogicalSpawnerMonster(logicalIndex, &position, &yawDeg);
#else
	UNREFERENCED_PARAMETER(megaGridNumber);
	UNREFERENCED_PARAMETER(kind);
	UNREFERENCED_PARAMETER(position);
	UNREFERENCED_PARAMETER(yawDeg);
	return nullptr;
#endif
}

CGameObject* CGameScene::SpawnLogicalPreviewEntry(const EnemySpawnerPreviewEntry& preview)
{
#ifndef USING_NETWORK
	if ( preview.logicalMonsterIndex < 0 )
		return nullptr;

	if ( preview.logicalMonsterIndex >= static_cast< int >(m_logicalMonsters.size()) )
		return nullptr;

	const LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(preview.logicalMonsterIndex)];

	if ( !logical.spawnerEntry )
		return nullptr;

	if ( logical.spawnerConsumed )
		return nullptr;

	if ( logical.kind != ConvertEnemySpawnerKindToLogicalKind(preview.kind) )
		return nullptr;

	if ( logical.megaGridNumber != preview.megaGridNumber )
		return nullptr;

	return ActivateLogicalSpawnerMonster(preview.logicalMonsterIndex, &preview.spawnPosition, &preview.yawDeg);
#else
	UNREFERENCED_PARAMETER(preview);
	return nullptr;
#endif
}

int CGameScene::SpawnLogicalMegaGrid(int megaGridNumber)
{
#ifndef USING_NETWORK
	int spawnedCount = 0;

	for ( ;; )
	{
		int logicalIndex = -1;

		for ( int i = 0; i < static_cast< int >(m_logicalMonsters.size()); ++i )
		{
			const LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(i)];

			if ( !logical.spawnerEntry )
				continue;

			if ( logical.spawnerConsumed )
				continue;

			if ( logical.active )
				continue;

			if ( logical.dead )
				continue;

			if ( logical.megaGridNumber != megaGridNumber )
				continue;

			logicalIndex = i;
			break;
		}

		if ( logicalIndex < 0 )
			break;

		if ( !ActivateLogicalSpawnerMonster(logicalIndex, nullptr, nullptr) )
			break;

		++spawnedCount;
	}

	return spawnedCount;
#else
	UNREFERENCED_PARAMETER(megaGridNumber);
	return 0;
#endif
}

void CGameScene::ConfigureLogicalSpawnerVisualRuntime(CGameObject* monster, int logicalMonsterIndex)
{
#ifndef USING_NETWORK
	if ( !monster )
		return;

	if ( logicalMonsterIndex < 0 || logicalMonsterIndex >= static_cast< int >(m_logicalMonsters.size()) )
		return;

	const LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalMonsterIndex)];

	DisableAllMonsterAIComponents(monster);

	if ( logical.dead || logical.hp <= 0 )
		return;

	auto ResetAndEnableAI = [ &logical ] (CMonsterAIComponent* ai, bool enabled)
		{
			if ( !ai )
				return;

			ai->ResetRuntimeStateForReuse(logical.homePosition, logical.yawDeg);
			ai->SetEnabledAI(enabled);
		};

	if ( logical.megaGridNumber == 5 && logical.kind != ELogicalMonsterKind::Boss )
	{
		CBossStageMonsterAIComponent* ai = monster->GetComponent<CBossStageMonsterAIComponent>();
		if ( !ai )
			ai = monster->AddComponent<CBossStageMonsterAIComponent>();

		if ( ai )
		{
			ai->SetScene(this);
			ai->ResetRuntimeStateForReuse(logical.homePosition, logical.yawDeg);

			switch ( logical.kind )
			{
			case ELogicalMonsterKind::SwordMan:
				ai->ConfigureBossStageMonsterAI(CBossStageMonsterAIComponent::EKind::SwordMan);
				break;
			case ELogicalMonsterKind::BowMan:
				ai->ConfigureBossStageMonsterAI(CBossStageMonsterAIComponent::EKind::BowMan);
				break;
			case ELogicalMonsterKind::Mutant:
				ai->ConfigureBossStageMonsterAI(CBossStageMonsterAIComponent::EKind::Mutant);
				break;
			case ELogicalMonsterKind::Ghoul:
			default:
				ai->ConfigureBossStageMonsterAI(CBossStageMonsterAIComponent::EKind::Ghoul);
				break;
			}

			ai->SetEnabledAI(m_bSimulateLocalBossStageMonsterAI);
		}

		return;
	}

	if ( logical.kind == ELogicalMonsterKind::Ghoul && logical.spawnerEntry && ( logical.megaGridNumber == 6 || logical.megaGridNumber == 8 ) )
	{
		CEnemySpawnerGhoulAIComponent* ai = monster->GetComponent<CEnemySpawnerGhoulAIComponent>();
		if ( !ai )
			ai = monster->AddComponent<CEnemySpawnerGhoulAIComponent>();

		if ( ai )
		{
			ai->SetScene(this);
			ai->ResetRuntimeStateForReuse(logical.homePosition, logical.yawDeg);
			ai->ConfigureSpawnerGhoulAI(logical.megaGridNumber, 60.0f);
			ai->SetEnabledAI(m_bSimulateLocalEnemySpawner);
		}

		return;
	}

	switch ( logical.kind )
	{
	case ELogicalMonsterKind::Ghoul:
	{
		CGhoulAIComponent* ai = monster->GetComponent<CGhoulAIComponent>();
		if ( !ai )
			ai = monster->AddComponent<CGhoulAIComponent>();

		if ( ai )
			ai->SetScene(this);

		ResetAndEnableAI(ai, m_bSimulateLocalGhoulAI);
		break;
	}
	case ELogicalMonsterKind::SwordMan:
	{
		CSwordManAIComponent* ai = monster->GetComponent<CSwordManAIComponent>();
		if ( !ai )
			ai = monster->AddComponent<CSwordManAIComponent>();

		if ( ai )
			ai->SetScene(this);

		ResetAndEnableAI(ai, m_bSimulateLocalSwordManAI);
		break;
	}
	case ELogicalMonsterKind::BowMan:
	{
		CBowManAIComponent* ai = monster->GetComponent<CBowManAIComponent>();
		if ( !ai )
			ai = monster->AddComponent<CBowManAIComponent>();

		if ( ai )
			ai->SetScene(this);

		ResetAndEnableAI(ai, m_bSimulateLocalBowManAI);
		break;
	}
	case ELogicalMonsterKind::Mutant:
	{
		CMutantAIComponent* ai = monster->GetComponent<CMutantAIComponent>();
		if ( !ai )
			ai = monster->AddComponent<CMutantAIComponent>();

		if ( ai )
			ai->SetScene(this);

		ResetAndEnableAI(ai, m_bSimulateLocalMutantAI);
		break;
	}
	default:
		break;
	}
#else
	UNREFERENCED_PARAMETER(monster);
	UNREFERENCED_PARAMETER(logicalMonsterIndex);
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
		StopAllLocalMonsterChaseAndReturnHome();
	}
	else
	{
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

	if ( auto* ai = monster->GetComponent<CEnemySpawnerGhoulAIComponent>() )
	{
		ai->ClearTarget();
		return;
	}

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
	auto ClearDynamicCollisionMask = [ ] (CGameObject* obj)
		{
			if ( !obj )
				return;

			CColliderComponent* collider = obj->GetComponent<CColliderComponent>();
			if ( !collider )
				return;

			if ( collider->IsCollisionMegaGridMaskFixed() )
				return;

			collider->SetCollisionMegaGridMask(0);
		};

	auto DisableDynamicCollisionObject = [ &ClearDynamicCollisionMask ] (CGameObject* obj)
		{
			if ( !obj )
				return;

			if ( auto* collider = obj->GetComponent<CColliderComponent>() )
			{
				collider->SetEnabled(false);
				collider->SetCollisionEnabled(false);

				if ( !collider->IsCollisionMegaGridMaskFixed() )
					collider->SetCollisionMegaGridMask(0);
			}

			if ( auto* monsterWeaponHitbox = obj->GetComponent<CMonsterWeaponHitboxComponent>() )
				monsterWeaponHitbox->SetEnabled(false);

			if ( auto* playerWeaponHitbox = obj->GetComponent<CWeaponHitboxComponent>() )
				playerWeaponHitbox->SetEnabled(false);
		};

	auto IsValidBoundLogicalMonster = [ this ] (CGameObject* monster, ELogicalMonsterKind expectedKind) -> bool
		{
			if ( !monster )
				return false;

			if ( !monster->GetActive() )
				return false;

			if ( IsMonsterDead(monster) )
				return false;

			const int logicalIndex = FindLogicalMonsterIndexByObject(monster);
			if ( logicalIndex < 0 || logicalIndex >= static_cast< int >(m_logicalMonsters.size()) )
				return false;

			const LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

			if ( logical.kind != expectedKind )
				return false;

			if ( logical.boundObject != monster )
				return false;

			if ( !logical.active )
				return false;

			if ( logical.dead || logical.hp <= 0 )
				return false;

			return true;
		};

	auto RefreshObject = [ this, &ClearDynamicCollisionMask ] (CGameObject* obj)
		{
			if ( !obj )
				return;

			CColliderComponent* collider = obj->GetComponent<CColliderComponent>();
			if ( !collider )
				return;

			if ( collider->IsCollisionMegaGridMaskFixed() )
				return;

			if ( !obj->GetActive() )
			{
				collider->SetCollisionMegaGridMask(0);
				return;
			}

			if ( !collider->IsCollisionEnabled() )
			{
				collider->SetCollisionMegaGridMask(0);
				return;
			}

			const uint16_t mask = ComputeObjectCurrentMegaGridMask(obj);
			collider->SetCollisionMegaGridMask(mask);
		};

	for ( size_t i = 0; i < m_EnemySwordRefs.size(); ++i )
	{
		CGameObject* sword = m_EnemySwordRefs[i];
		CGameObject* owner = ( i < m_swordManRefs.size() ) ? m_swordManRefs[i] : nullptr;

		if ( !IsValidBoundLogicalMonster(owner, ELogicalMonsterKind::SwordMan) )
			DisableDynamicCollisionObject(sword);
	}

	for ( size_t i = 0; i < m_EnemyBowRefs.size(); ++i )
	{
		CGameObject* bow = m_EnemyBowRefs[i];
		CGameObject* owner = ( i < m_bowManRefs.size() ) ? m_bowManRefs[i] : nullptr;

		if ( !IsValidBoundLogicalMonster(owner, ELogicalMonsterKind::BowMan) )
			DisableDynamicCollisionObject(bow);
	}

	for ( size_t i = 0; i < m_helmetRefs.size(); ++i )
	{
		CGameObject* helmet = m_helmetRefs[i];
		CGameObject* owner = ( i < m_MutantRefs.size() ) ? m_MutantRefs[i] : nullptr;

		if ( !IsValidBoundLogicalMonster(owner, ELogicalMonsterKind::Mutant) )
			DisableDynamicCollisionObject(helmet);
	}

	for ( CGameObject* player : m_playersBySlot )
		RefreshObject(player);

	for ( CGameObject* obj : m_staticGameplayTickObjects )
		RefreshObject(obj);

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
	UpdateCastleCenterMegaGridState();
}

bool CGameScene::IsLocalPlayerInsideCastleCenterMegaGridFullArea() const
{
	CGameObject* player = GetPlayer();

	if ( !player )
		player = GetPlayerBySlot(0);

	if ( !player )
		return false;

	const XMFLOAT3 pos = player->GetPosition();

	return IsWorldPositionInsideMegaGrid5CenterSquare250(pos.x, pos.z);
}

void CGameScene::UpdateCastleCenterMegaGridState()
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	m_bLocalPlayerInsideCastleCenterMegaGrid = IsLocalPlayerInsideCastleCenterMegaGridFullArea();
}

bool CGameScene::ShouldUseBossStageBgm() const
{
	if ( m_bLocalPlayerDead )
		return false;

	CGameObject* boss = FindBossStageBossInMegaGrid(5);

	if ( !boss )
		return false;

	if ( IsMonsterDead(boss) )
		return false;

	const bool bossActive = m_bBossStageBossActivated && boss->GetActive();

	const float bossAppearLeadSeconds = 1.0f;
	const float summonBgmStartAge = std::max(0.0f, kBossSummonCircleFadeInDurationSec - bossAppearLeadSeconds);

	const bool bossWillAppearSoon =
		m_bBossSummonSequenceStarted &&
		!m_bBossStageBossActivated &&
		m_pendingBossStageBoss == boss &&
		m_bBossSummonCircleFadeAgeSec >= summonBgmStartAge;

	if ( !bossActive && !bossWillAppearSoon )
		return false;

	return IsLocalPlayerInsideCastleCenterMegaGridFullArea();
}

void CGameScene::UpdateBossStageBgmState()
{
	if ( !m_pAudioManager )
		return;

	CMusicDirector* music = m_pAudioManager->GetMusicDirector();

	if ( !music )
		return;

	const bool shouldUseBossStageBgm = ShouldUseBossStageBgm();

	if ( shouldUseBossStageBgm == m_bBossStageBgmActive )
		return;

	m_bBossStageBgmActive = shouldUseBossStageBgm;

	music->SetCrossFadeSeconds(1.5f);
	music->RequestState(shouldUseBossStageBgm ? EMusicState::Boss : EMusicState::Gameplay, false);
	music->BeginPendingTransition();
}

void CGameScene::DumpStaticGridOccupancyLog() const
{
	m_sceneGrid.DumpStaticGridOccupancyLog();
}

const std::vector<CGameObject*>& CGameScene::GetMegaGridMonstersByWorldPosition(
	const XMFLOAT3& worldPos) const
{
	static const std::vector<CGameObject*> kEmpty;

	if ( !m_sceneGrid.IsInitialized() )
		return kEmpty;

	int megaX = -1;
	int megaZ = -1;

	if ( !m_sceneGrid.TryGetMegaGridFromWorldPosition(
		worldPos.x,
		worldPos.z,
		megaX,
		megaZ) )
	{
		return kEmpty;
	}

	return m_sceneGrid.GetMegaGridMonsters(megaX, megaZ);
}

void CGameScene::NotifyMonsterChaseStarted(CGameObject* monster)
{
#ifndef USING_NETWORK
	if ( !monster )
		return;

	const int logicalIndex = FindLogicalMonsterIndexByObject(monster);
	if ( logicalIndex < 0 || logicalIndex >= static_cast< int >(m_logicalMonsters.size()) )
		return;

	const LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

	if ( !logical.keyTrigger )
		return;

	const int megaGridNumber = logical.keyTriggerMegaGridNumber;
	if ( megaGridNumber != 6 && megaGridNumber != 8 )
		return;

	TryRunEnemySpawnerEventForMegaGrid(megaGridNumber);
#else
	UNREFERENCED_PARAMETER(monster);
#endif
}

void CGameScene::SetMegaGridApproachZoneSize(int megaX, int megaZ, int widthCells, int heightCells)
{
	m_sceneGrid.SetMegaGridApproachZoneSize(megaX, megaZ, widthCells, heightCells);
}

void CGameScene::SetMegaGridCleared(int megaX, int megaZ, bool cleared)
{
	m_sceneGrid.SetMegaGridCleared(megaX, megaZ, cleared);
	RefreshPlayerWeaponDamageTierFromClearedMegaGrids();
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

void CGameScene::ReleaseSkyBoxResources()
{
	m_skyBox.shader.reset();
	m_skyBox.texture.reset();
	m_skyBox.vertexBuffer.Reset();
	m_skyBox.vertexUploadBuffer.Reset();
	m_skyBox.vertexBufferView = {};
	m_skyBox.vertexCount = 0;
	m_skyBox.textureBaseSrvIndex = UINT_MAX;
	m_skyBox.objectCB = {};
}

void CGameScene::ReleaseSkyBoxUploadBuffers()
{
	m_skyBox.vertexUploadBuffer.Reset();

	if ( m_skyBox.texture )
		m_skyBox.texture->ReleaseUploadBuffers();
}

void CGameScene::ReleaseObjects()
{
	ReleaseSkyBoxResources();

	m_staticBatch.shader.reset();
	m_skinnedBatch.shader.reset();

	m_treeStaticShader.reset();
	m_transparentWaterShader.reset();
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

	m_terrainObjects.clear();
	m_staticObjects.clear();
	m_skinnedObjects.clear();

	m_lightObjects.clear();
	m_pPlayerSpotFollower = nullptr;

	m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };
	m_otherPlayerWorldHpGaugeVisibleForHud.fill(false);

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
	m_monsterHpGaugeRuntimeStates.clear();
	m_bossStageBossPositionStates.clear();

	m_bBossStageBossActivated = false;
	m_bBossSummonSequenceStarted = false;
	m_bBossSummonCircleFadeAgeSec = 0.0f;
	m_pendingBossStageBoss = nullptr;
#ifdef USING_NETWORK
	m_bossSummonVisualCenter = XMFLOAT3(0.0f, 0.0f, 400.0f);
#endif

	m_bBossSummonVisualFadeOutStarted = false;
	m_bBossSummonVisualFadeOutAgeSec = 0.0f;
	m_bBossShockwaveActive = false;
	m_bossShockwaveAgeSec = 0.0f;
	m_bossShockwaveCenter = XMFLOAT3(0.0f, 0.0f, 0.0f);

	m_bBossShockwavePushLocalPlayer = false;
	m_bossShockwavePrevRadius = 0.0f;
	m_bossShockwavePlayerInitialDistance = 0.0f;
	m_bossShockwavePlayerPushDir = XMFLOAT3(0.0f, 0.0f, 1.0f);

	ResetBossShockwaveWindSfxTracking();

	m_bossPoisonProjectileEffect.entries.clear();
	m_bossPoisonProjectileEffect.spellCastStates.clear();
	m_bossMeleeSlashCastStates.clear();

	m_bBossStageBossActivated = false;
	m_bBossSummonSequenceStarted = false;
	m_bBossSummonCircleFadeAgeSec = 0.0f;
	m_pendingBossStageBoss = nullptr;
#ifdef USING_NETWORK
	m_bossSummonVisualCenter = XMFLOAT3(0.0f, 0.0f, 400.0f);
#endif

	m_mutantKeyTriggerMegaByObject.clear();
	m_mutantKeyTriggerRegisteredByMega.fill(false);

	m_helmetRefs.clear();
	m_arrowRefs.clear();
	m_bulletRefs.clear();
	m_networkArrowById.clear();
	m_networkBulletById.clear();
	m_networkBossPoisonById.clear();

#ifdef USING_NETWORK
	m_enemyDRStates.clear();
	m_playerDRStates.clear();
	m_projectileDRStates.clear();
#endif

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

	m_playerWeaponOwnerByObject.clear();

	m_EnemySwordRefs.clear();
	m_EnemyBowRefs.clear();

	m_EnemySpawnRefs.clear();
	m_enemySpawnPoolEntries.clear();
	m_enemySpawner.reset();

	ResetEnemySpawnerTimedGhoulWaveStates();

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
	m_shadowTerrainShader.reset();

	m_sceneRenderTargetCount = 0;
	m_bSceneRenderTargetsReady = false;
	m_bInactiveOverlayVisible = false;
	m_bStartedGameplayMusic = false;
	m_bBossStageBgmActive = false;
	m_bWasLocalPlayerInsideMegaGridCenter = false;
	m_bLocalPlayerInsideCastleCenterMegaGrid = false;

	m_navMesh.reset();

	m_bLocalPlayerDead = false;
	m_bLocalPlayerRespawnUsed = false;
	m_localPlayerRespawnTimer = 0.0f;

#ifdef USING_NETWORK
	m_prevPlayerNetworkStateCode.clear();
	m_prevEnemyNetworkStateCode.clear();
	m_networkBossCallIndex = 0;
	m_networkBossCallPendingSummonEffects = 0;
	m_networkBossCallSummonEffectWindowSec = 0.0f;
	m_networkBossCallSummonVisualPreviews.clear();
	m_networkBossCallSummonPreviewKeys.clear();
	m_networkBossCallSummonEffectEnemyIds.clear();
	m_prevNetworkEnemyPositions.clear();
	m_playedSpawnFxKeys.clear();
#endif

	m_playerWeaponDamageTierIndex = 0;
	m_deadMonsters.clear();
	m_skinnedMonsterMegaGridNumbers.clear();
	ResetLogicalMonsterState();

	m_itemBillboardState.shader.reset();
	m_itemBillboardState.transparentShader.reset();
	m_itemBillboardState.quadMesh.reset();
	m_itemBillboardState.keyTexture.reset();

	for ( std::shared_ptr<CTexture>& potionTexture : m_itemBillboardState.potionTextures )
		potionTexture.reset();

	m_itemBillboardState.bossSummonCircleTexture.reset();
	m_itemBillboardState.entries.clear();
	m_itemBillboardState.activeBossCallSummonCircleItemIndices.clear();
	m_itemBillboardState.bossCallSummonCircleVisual = BossCallSummonCircleVisualState{};
	m_itemBillboardState.bossSummonGlowParticleEmitAccumulatorSec = 0.0f;
	m_itemBillboardState.bossCallSummonGlowParticleEmitAccumulatorSec = 0.0f;

	for ( std::array<float, CGameSceneHUD::kInventorySlotCount>& accumulators : m_inventoryBuffParticleEmitAccumulators )
		accumulators.fill(0.0f);

	m_megaGrid4LowYPoisonStates = {};

	m_bossCallSummonPlanCallIndex = -1;
	m_bossCallSummonPlanEntries.clear();

	ReleaseAllGameSceneEffectGpuResources();

	m_muzzleFlashEffect.shader.reset();
	m_muzzleFlashEffect.entries.clear();

	m_gunSmokeEffect.shader.reset();
	m_gunSmokeEffect.entries.clear();

	m_bossPoisonProjectileEffect.shader.reset();

	m_swordTrailEffect.shader.reset();
	m_swordTrailEffect.entries.clear();

	m_monsterSwordTrailEffect.shader.reset();
	m_monsterSwordTrailEffect.entries.clear();

	m_arrowTrailEffect.shader.reset();
	m_arrowTrailEffect.entries.clear();

	m_monsterArrowTrailEffect.shader.reset();
	m_monsterArrowTrailEffect.entries.clear();

	m_bossCallSummonWwwEffect.shader.reset();
	m_bossCallSummonWwwEffect.entries.clear();

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
	ReleaseSkyBoxUploadBuffers();

	for ( UINT j = 0; j < ( UINT ) m_staticObjects.size(); ++j )
	{
		if ( !m_staticObjects[j] ) continue;
		m_staticObjects[j]->ReleaseUploadBuffers();
	}

	for ( UINT j = 0; j < ( UINT ) m_skinnedObjects.size(); ++j )
	{
		if ( !m_skinnedObjects[j] ) continue;
		m_skinnedObjects[j]->ReleaseUploadBuffers();
	}

#ifdef _WITH_BATCH_MATERIAL
	if ( m_staticBatch.material ) m_staticBatch.material->ReleaseUploadBuffers();
#endif

	if ( m_itemBillboardState.quadMesh )
		m_itemBillboardState.quadMesh->ReleaseUploadBuffers();

	if ( m_itemBillboardState.keyTexture )
		m_itemBillboardState.keyTexture->ReleaseUploadBuffers();

	for ( std::shared_ptr<CTexture>& potionTexture : m_itemBillboardState.potionTextures )
	{
		if ( potionTexture )
			potionTexture->ReleaseUploadBuffers();
	}

	if ( m_itemBillboardState.bossSummonCircleTexture )
		m_itemBillboardState.bossSummonCircleTexture->ReleaseUploadBuffers();

	if ( m_monsterHpGaugeState.quadMesh )
		m_monsterHpGaugeState.quadMesh->ReleaseUploadBuffers();

	if ( m_monsterHpGaugeState.hpTexture )
		m_monsterHpGaugeState.hpTexture->ReleaseUploadBuffers();

	if ( m_monsterHpGaugeState.emptyHpTexture )
		m_monsterHpGaugeState.emptyHpTexture->ReleaseUploadBuffers();

	for ( auto& texture : m_monsterHpGaugeState.playerNameTextures )
	{
		if ( texture )
			texture->ReleaseUploadBuffers();
	}
}

void CGameScene::ReleaseShaderVariables()
{
	ReleaseAllGameSceneEffectGpuResources();
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
	ReleaseSsaoConstantBuffer();

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
	ReleaseSsaoResources();
}

void CGameScene::ReleaseSsaoConstantBuffer()
{
	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		if ( m_pd3dcbSsao[frameIndex] )
		{
			if ( m_pcbMappedSsao[frameIndex] )
			{
				m_pd3dcbSsao[frameIndex]->Unmap(0, nullptr);
				m_pcbMappedSsao[frameIndex] = nullptr;
			}

			m_pd3dcbSsao[frameIndex].Reset();
		}

		m_pcbMappedSsao[frameIndex] = nullptr;
	}

	m_nSsaoCBElementBytes = 0;
}

void CGameScene::ReleaseSsaoResources()
{
	mSsaoShader.reset();
	mSsaoBlurShader.reset();
	mSsaoNormalMap.reset();
	mSsaoAmbientMap0.reset();
	mSsaoAmbientMap1.reset();
	mSsaoRandomVectorMap.reset();
	mSsao.reset();

	mSsaoNormalMapSrvIndex = UINT_MAX;
	mSsaoSceneNormalMapSrvIndex = UINT_MAX;
	mSsaoAmbientMap0SrvIndex = UINT_MAX;
	mSsaoAmbientMap1SrvIndex = UINT_MAX;
	mSsaoRandomVectorMapSrvIndex = UINT_MAX;
	mSsaoDepthMapSrvIndex = UINT_MAX;
	mSsaoRtvHandles = {};
	m_bSsaoResourcesReady = false;
	m_bSsaoRtvsReady = false;
	m_pd3dSsaoDevice = nullptr;
}

void CGameScene::UpdateSsaoCB(CCamera* camera)
{
	if ( !mSsao || !camera )
		return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;
	SsaoCB* mappedSsao = m_pcbMappedSsao[frameIndex];

	if ( !mappedSsao )
		return;

	SsaoCB ssaoCB{};

	const XMFLOAT4X4 proj = camera->GetProjectionMatrix();
	const XMMATRIX P = XMLoadFloat4x4(&proj);
	const XMMATRIX invP = XMMatrixInverse(nullptr, P);

	const XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	);

	XMStoreFloat4x4(&ssaoCB.Proj, XMMatrixTranspose(P));
	XMStoreFloat4x4(&ssaoCB.InvProj, XMMatrixTranspose(invP));
	XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(P * T));

	mSsao->GetOffsetVectors(ssaoCB.OffsetVectors);

	const std::vector<float> blurWeights = mSsao->CalcGaussWeights(2.5f);
	if ( blurWeights.size() >= 11 )
	{
		ssaoCB.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
		ssaoCB.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
		ssaoCB.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);
	}

	const UINT ssaoWidth = mSsao->SsaoMapWidth();
	const UINT ssaoHeight = mSsao->SsaoMapHeight();

	if ( ssaoWidth > 0 && ssaoHeight > 0 )
	{
		ssaoCB.InvRenderTargetSize = XMFLOAT2(
			1.0f / static_cast< float >( ssaoWidth ),
			1.0f / static_cast< float >( ssaoHeight )
		);
	}

	ssaoCB.OcclusionRadius = 0.5f;
	ssaoCB.OcclusionFadeStart = 0.2f;
	ssaoCB.OcclusionFadeEnd = 1.0f;
	ssaoCB.SurfaceEpsilon = 0.05f;

	ssaoCB.NormalMapIndex = mSsaoSceneNormalMapSrvIndex;
	ssaoCB.DepthMapIndex = mSsaoDepthMapSrvIndex;
	ssaoCB.RandomVecMapIndex = mSsaoRandomVectorMapSrvIndex;
	ssaoCB.InputMapIndex = mSsaoAmbientMap0SrvIndex;
	ssaoCB.HorizontalBlur = 0;

	*mappedSsao = ssaoCB;
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

	const bool shouldCullTrees = ShouldCullTreesByVillageDoorProbes(camera);

	if ( !shouldCullTrees )
		return;

	for ( UINT objectIndex : m_staticTreeObjectIndices )
	{
		if ( objectIndex >= static_cast< UINT >( m_staticTreeGridCullFlags.size() ) )
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
			bool useTreeShader,
			bool useTerrainShader,
			bool useWaterShader)
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
				key.useTerrainShader = useTerrainShader;
				key.useWaterShader = useWaterShader;
				key.lodLevel = lodLevel;

				size_t groupIndex = 0;

				auto it = groupIndexByKey.find(key);

				if ( it == groupIndexByKey.end() )
				{
					StaticInstanceGroup group{};
					group.mesh = mesh;
					group.subMeshIndex = subMeshIndex;
					group.useTreeShader = useTreeShader;
					group.useTerrainShader = useTerrainShader;
					group.useWaterShader = useWaterShader;
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

		const bool useTerrainShader =
			( m_terrainObjects.find(obj) != m_terrainObjects.end() );

		const bool useWaterShader =
			( m_waterObjects.find(obj) != m_waterObjects.end() );

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
						useTreeShader,
						useTerrainShader,
						useWaterShader
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
			AddObjectToGroup(objectIndex, mesh, 0, useTreeShader, useTerrainShader, useWaterShader);
		}
	}

	std::sort(
		m_staticInstanceGroups.begin(),
		m_staticInstanceGroups.end(),
		[ ] (const StaticInstanceGroup& a, const StaticInstanceGroup& b)
		{
			if ( a.useTreeShader != b.useTreeShader )
				return a.useTreeShader < b.useTreeShader;

			if ( a.useTerrainShader != b.useTerrainShader )
				return a.useTerrainShader < b.useTerrainShader;

			if ( a.useWaterShader != b.useWaterShader )
				return a.useWaterShader < b.useWaterShader;

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

			if ( m_bLocalPlayerInsideCastleCenterMegaGrid &&
				m_terrainObjects.find(cache.object) != m_terrainObjects.end() )
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

			if ( m_bLocalPlayerInsideCastleCenterMegaGrid &&
				m_terrainObjects.find(cache.object) != m_terrainObjects.end() )
			{
				continue;
			}

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

	int lastShaderKind = -1; // 0=static, 1=tree, 2=terrain

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for ( const StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		if ( group.useWaterShader )
			continue;

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

		const int shaderKind =
			group.useWaterShader ? 3 :
			group.useTerrainShader ? 2 :
			group.useTreeShader ? 1 :
			0;

		if ( lastShaderKind != shaderKind )
		{
			if (shaderKind == 3)
			{
				if (m_waterShader)
					m_waterShader->Render(cmd, camera, &m_staticBatch);
				else if (m_staticBatch.shader)
					m_staticBatch.shader->Render(cmd, camera, &m_staticBatch);
			}
			else if ( shaderKind == 2 )
			{
				if ( m_terrainShader )
					m_terrainShader->Render(cmd, camera, &m_staticBatch);
				else if ( m_staticBatch.shader )
					m_staticBatch.shader->Render(cmd, camera, &m_staticBatch);
			}
			else if ( shaderKind == 1 )
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

			lastShaderKind = shaderKind;
		}

		const UINT mid = ( sm.materialId == 0xFFFFFFFFu ) ? 0u : sm.materialId;
		cmd->SetGraphicsRoot32BitConstant(ROOT_PARAMETER_MATERIAL_ID, mid, 0);

		if ( sm.material && sm.material->NeedsLegacyBinding() )
			sm.material->UpdateShaderVariables(cmd);

		cmd->IASetVertexBuffers(0, 2, vbViews);
		cmd->IASetIndexBuffer(&sm.ibView);

		cmd->IASetPrimitiveTopology(
			(group.useTerrainShader || group.useWaterShader)
			? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
			: D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
		);

		cmd->DrawIndexedInstanced(( UINT ) sm.indices.size(), visibleInstanceCount, 0, 0, 0);
	}
}

void CGameScene::RenderTransparentWaterInstanceGroups(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::RenderTransparentWaterInstanceGroups");

	if ( !cmd ) return;
	if ( !m_transparentWaterShader ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	ID3D12Resource* staticInstanceBuffer =
		m_pd3dStaticInstanceBuffer[frameIndex].Get();

	StaticInstanceVertex* mappedStaticInstanceBuffer =
		m_pMappedStaticInstanceBuffer[frameIndex];

	if ( !staticInstanceBuffer ) return;
	if ( !mappedStaticInstanceBuffer ) return;

	m_transparentWaterShader->Render(cmd, camera, &m_staticBatch);

	for ( const StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		if ( !group.useWaterShader )
			continue;

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

		const UINT mid = ( sm.materialId == 0xFFFFFFFFu ) ? 0u : sm.materialId;
		cmd->SetGraphicsRoot32BitConstant(ROOT_PARAMETER_MATERIAL_ID, mid, 0);

		if ( sm.material && sm.material->NeedsLegacyBinding() )
			sm.material->UpdateShaderVariables(cmd);

		cmd->IASetVertexBuffers(0, 2, vbViews);
		cmd->IASetIndexBuffer(&sm.ibView);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd->DrawIndexedInstanced(( UINT ) sm.indices.size(), visibleInstanceCount, 0, 0, 0);
	}
}

float CGameScene::GetTerrainGroundYOrFallback(float worldX, float worldZ, float fallbackY) const
{
	if ( !m_TerrainData )
		return fallbackY;

	const XMFLOAT3 terrainWorldPosition = m_TerrainData->GetWorldPosition();
	const float localX = worldX - terrainWorldPosition.x;
	const float localZ = worldZ - terrainWorldPosition.z;

	if ( localX < 0.0f || localZ < 0.0f || localX > m_TerrainData->GetWorldWidth() || localZ > m_TerrainData->GetWorldLength() )
		return fallbackY;

	return terrainWorldPosition.y + m_TerrainData->GetHeight(localX, localZ);
}

XMFLOAT3 CGameScene::AlignPositionYToTerrainGround(const XMFLOAT3& position, float yOffset) const
{
	XMFLOAT3 adjusted = position;
	adjusted.y = GetTerrainGroundYOrFallback(position.x, position.z, position.y) + yOffset;
	return adjusted;
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
			if ( !obj ) continue;
			if ( !obj->GetActive() ) continue;

			if ( IsBossMonsterObject(obj) && !IsBossStageBossRenderAllowed(obj) )
				continue;

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
	if ( !m_shadowTerrainShader ) return;

	int lastShaderKind = -1; // 0=static, 1=tree alpha-clip, 2=terrain
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

		const int shaderKind =
			group.useTerrainShader ? 2 :
			group.useTreeShader ? 1 :
			0;

		if ( !hasBoundAnyShader || lastShaderKind != shaderKind )
		{
			if ( shaderKind == 2 )
				m_shadowTerrainShader->Render(cmd, nullptr, &m_staticBatch);
			else if ( shaderKind == 1 )
				m_shadowAlphaClipStaticShader->Render(cmd, nullptr, &m_staticBatch);
			else
				m_shadowStaticShader->Render(cmd, nullptr, &m_staticBatch);

			lastShaderKind = shaderKind;
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

		cmd->IASetPrimitiveTopology(
			group.useTerrainShader
			? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
			: D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
		);

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

			CGameObject* obj = m_skinnedBatch.objectRefs[objectIndex];
			if ( !obj ) continue;
			if ( !obj->GetActive() ) continue;

			if ( IsBossMonsterObject(obj) && !IsBossStageBossRenderAllowed(obj) )
				continue;

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

void CGameScene::RenderSsao(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd || !camera )
		return;

	if ( !m_bSsaoResourcesReady || !m_bSsaoRtvsReady )
		return;

	if ( !mSsao || !mSsaoShader || !mSsaoBlurShader )
		return;

	if ( !mSsaoAmbientMap0 || !mSsaoAmbientMap1 )
		return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;
	SsaoCB* mappedSsao = m_pcbMappedSsao[frameIndex];

	if ( !mappedSsao || !m_pd3dcbSsao[frameIndex] )
		return;

	if ( mappedSsao->NormalMapIndex == UINT_MAX ||
		 mappedSsao->DepthMapIndex == UINT_MAX ||
		 mSsaoAmbientMap0SrvIndex == UINT_MAX ||
		 mSsaoAmbientMap1SrvIndex == UINT_MAX )
		return;

	auto bindSsaoRootState = [this, cmd, frameIndex]()
	{
		if ( m_pDescriptorHeap )
		{
			cmd->SetGraphicsRootDescriptorTable(
				ROOT_PARAMETER_GLOBAL_SRV,
				m_pDescriptorHeap->GetGPUSrvDescriptorStartHandle()
			);
		}

		cmd->SetGraphicsRootConstantBufferView(
			ROOT_PARAMETER_SSAO,
			m_pd3dcbSsao[frameIndex]->GetGPUVirtualAddress()
		);
	};

	cmd->RSSetViewports(1, &mSsao->Viewport());
	cmd->RSSetScissorRects(1, &mSsao->ScissorRect());

	::SynchronizeResourceTransition(
		cmd,
		mSsaoAmbientMap0->GetResource(0),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	const float clearValue[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	cmd->ClearRenderTargetView(mSsaoRtvHandles[1], clearValue, 0, nullptr);
	cmd->OMSetRenderTargets(1, &mSsaoRtvHandles[1], TRUE, nullptr);

	mappedSsao->InputMapIndex = UINT_MAX;
	mappedSsao->HorizontalBlur = 0;

	mSsaoShader->OnPrepareRender(cmd);
	bindSsaoRootState();
	cmd->IASetVertexBuffers(0, 0, nullptr);
	cmd->IASetIndexBuffer(nullptr);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(6, 1, 0, 0);

	::SynchronizeResourceTransition(
		cmd,
		mSsaoAmbientMap0->GetResource(0),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_GENERIC_READ
	);

	for ( int blurIndex = 0; blurIndex < 3; ++blurIndex )
	{
		struct BlurPass
		{
			CTexture* input = nullptr;
			CTexture* output = nullptr;
			D3D12_CPU_DESCRIPTOR_HANDLE outputRtv = {};
			UINT inputSrvIndex = UINT_MAX;
			UINT horizontal = 0;
		};

		const BlurPass passes[2] =
		{
			{ mSsaoAmbientMap0.get(), mSsaoAmbientMap1.get(), mSsaoRtvHandles[2], mSsaoAmbientMap0SrvIndex, 1 },
			{ mSsaoAmbientMap1.get(), mSsaoAmbientMap0.get(), mSsaoRtvHandles[1], mSsaoAmbientMap1SrvIndex, 0 }
		};

		for ( const BlurPass& pass : passes )
		{
			if ( !pass.output )
				continue;

			mappedSsao->InputMapIndex = pass.inputSrvIndex;
			mappedSsao->HorizontalBlur = pass.horizontal;

			::SynchronizeResourceTransition(
				cmd,
				pass.output->GetResource(0),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);

			cmd->ClearRenderTargetView(pass.outputRtv, clearValue, 0, nullptr);
			cmd->OMSetRenderTargets(1, &pass.outputRtv, TRUE, nullptr);

			mSsaoBlurShader->OnPrepareRender(cmd);
			bindSsaoRootState();
			cmd->IASetVertexBuffers(0, 0, nullptr);
			cmd->IASetIndexBuffer(nullptr);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmd->DrawInstanced(6, 1, 0, 0);

			::SynchronizeResourceTransition(
				cmd,
				pass.output->GetResource(0),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_GENERIC_READ
			);
		}
	}

	RestoreSceneRenderTargets(cmd, camera);
	BindFrameRootParameters(cmd);
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

	const UINT sceneMrtCount = std::min<UINT>(m_sceneRenderTargetCount, 5);

	cmd->OMSetRenderTargets(
		sceneMrtCount,
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

void CGameScene::SetBossSummonCircleDiffuseSrvIndex(UINT srvIndex)
{
	SetMaterialDiffuseSrvIndex(
		static_cast< int >( kBossSummonCircleMaterialId ),
		srvIndex
	);
}

void CGameScene::SetBossCallSummonCircleDiffuseSrvIndex(UINT srvIndex)
{
	SetMaterialDiffuseSrvIndex(
		static_cast< int >( kBossCallSummonCircleMaterialId ),
		srvIndex
	);
}

void CGameScene::SetBossSummonCircleAlpha(float alpha)
{
	if ( !m_pMaterials )
		return;

	alpha = std::clamp(alpha, 0.0f, 1.0f);

	MATERIAL& mat =
		m_pMaterials->m_pReflections[kBossSummonCircleMaterialId];

	mat.m_xmf4Diffuse.w = alpha;
}

void CGameScene::SetBossSummonGlowAlpha(float alpha)
{
	if ( !m_pMaterials )
		return;

	alpha = std::clamp(alpha, 0.0f, 1.0f);

	MATERIAL& mat =
		m_pMaterials->m_pReflections[kBossSummonGlowMaterialId];

	mat.m_xmf4Diffuse.w = alpha * 0.30f;
}

void CGameScene::SetBossShockwaveAlpha(float alpha)
{
	if ( !m_pMaterials )
		return;

	alpha = std::clamp(alpha, 0.0f, 1.0f);

	MATERIAL& mat =
		m_pMaterials->m_pReflections[kBossShockwaveMaterialId];

	mat.m_xmf4Diffuse.w = alpha;
}

void CGameScene::SetBossShockwaveWallAlpha(float alpha)
{
	if ( !m_pMaterials )
		return;

	alpha = std::clamp(alpha, 0.0f, 1.0f);

	MATERIAL& mat =
		m_pMaterials->m_pReflections[kBossShockwaveWallMaterialId];

	mat.m_xmf4Diffuse.w = alpha;
}

void CGameScene::SetBossSummonVisualAlpha(float alpha)
{
	alpha = std::clamp(alpha, 0.0f, 1.0f);

	SetBossSummonCircleAlpha(alpha);
	SetBossSummonGlowAlpha(alpha);
}

void CGameScene::SetBossSummonVisualActive(bool active)
{
	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( item.kind != EItemBillboardKind::BossSummonCircle &&
			 item.kind != EItemBillboardKind::BossSummonGlow )
		{
			continue;
		}

		item.active = active;
		item.distanceCulled = !active;
	}
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

CGameObject* CGameScene::ResolvePlayerAttackerFromPlayerWeapon(CGameObject* weaponObject) const
{
	if ( !weaponObject )
		return nullptr;

	// 혹시 weaponObject 자체가 플레이어인 경우.
	const int directPlayerSlot = GetPlayerSlotFromObject(weaponObject);
	if ( directPlayerSlot >= 0 && directPlayerSlot < 4 )
		return GetPlayerBySlot(directPlayerSlot);

	// 화살/총알처럼 풀에서 재사용되는 오브젝트는 발사 시점에 owner map에 기록한다.
	auto ownerIt = m_playerWeaponOwnerByObject.find(weaponObject);
	if ( ownerIt != m_playerWeaponOwnerByObject.end() )
	{
		CGameObject* owner = ownerIt->second;

		const int ownerSlot = GetPlayerSlotFromObject(owner);
		if ( ownerSlot >= 0 && ownerSlot < 4 )
			return owner;
	}

	// 검/도끼 같은 장착 무기는 slot 순서 ref로 역추적한다.
	for ( int slot = 0; slot < 4; ++slot )
	{
		const size_t index = static_cast< size_t >(slot);

		if ( index < m_PlayerSwordRefs.size() &&
			 m_PlayerSwordRefs[index] == weaponObject )
		{
			return GetPlayerBySlot(slot);
		}

		if ( index < m_PlayerAxeRefs.size() &&
			 m_PlayerAxeRefs[index] == weaponObject )
		{
			return GetPlayerBySlot(slot);
		}

		if ( index < m_PlayerBowRefs.size() &&
			 m_PlayerBowRefs[index] == weaponObject )
		{
			return GetPlayerBySlot(slot);
		}

		if ( index < m_PlayerGunRefs.size() &&
			 m_PlayerGunRefs[index] == weaponObject )
		{
			return GetPlayerBySlot(slot);
		}

		if ( index < m_preparedPlayerArrows.size() &&
			 m_preparedPlayerArrows[index] == weaponObject )
		{
			return GetPlayerBySlot(slot);
		}
	}

	return nullptr;
}

bool CGameScene::ForceMonsterAIChaseTarget(CGameObject* monster, CGameObject* target) const
{
	if ( !monster || !target )
		return false;

	if ( auto* ai = monster->GetComponent<CBossStageMonsterAIComponent>() )
		return ai->ForceChaseTarget(target);

	if ( auto* ai = monster->GetComponent<CEnemySpawnerGhoulAIComponent>() )
		return ai->ForceChaseTarget(target);

	if ( auto* ai = monster->GetComponent<CGhoulAIComponent>() )
		return ai->ForceChaseTarget(target);

	if ( auto* ai = monster->GetComponent<CSwordManAIComponent>() )
		return ai->ForceChaseTarget(target);

	if ( auto* ai = monster->GetComponent<CBowManAIComponent>() )
		return ai->ForceChaseTarget(target);

	if ( auto* ai = monster->GetComponent<CMutantAIComponent>() )
		return ai->ForceChaseTarget(target);

	if ( auto* ai = monster->GetComponent<CBossAIComponent>() )
		return ai->ForceChaseTarget(target);

	if ( auto* ai = monster->GetComponent<CMonsterAIComponent>() )
		return ai->ForceChaseTarget(target);

	return false;
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

		SetObjectAttackPower(arrowObj, GetPlayerArrowAttackPower(slot));

		m_playerWeaponOwnerByObject[arrowObj] = shooter;

		arrow->Prepare(bowObj, shooter, pullBackDistance, true, true);
		m_preparedPlayerArrows[( size_t ) slot] = arrowObj;
		return;
    }
}

void CGameScene::RequestPrepareBowmanArrow(CGameObject* bowman, float pullBackDistance)
{
	if ( !bowman ) return;

	if ( !bowman->GetActive() ) return;

	const int logicalIndex = FindLogicalMonsterIndexByObject(bowman);
	if ( logicalIndex < 0 || logicalIndex >= static_cast< int >(m_logicalMonsters.size()) ) return;

	const LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

	if ( logical.kind != ELogicalMonsterKind::BowMan ) return;
	if ( logical.boundObject != bowman ) return;
	if ( !logical.active ) return;
	if ( logical.dead || logical.hp <= 0 ) return;

	const int bowmanIndex = GetBowManIndexFromObject(bowman);
	if ( bowmanIndex < 0 ) return;

	const size_t idx = static_cast< size_t >(bowmanIndex);

	if ( idx >= m_preparedBowmanArrows.size() ) return;
	if ( idx >= m_EnemyBowRefs.size() ) return;

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

		m_playerWeaponOwnerByObject.erase(arrowObj);

		arrow->Prepare(bowObj, bowman, pullBackDistance, false, true);
		m_preparedBowmanArrows[idx] = arrowObj;
		return;
	}
}

void CGameScene::RequestReleasePreparedBowmanArrow(CGameObject* bowman, float speed, float lifeSec)
{
	if ( !bowman ) return;

	if ( !bowman->GetActive() ) return;

	const int logicalIndex = FindLogicalMonsterIndexByObject(bowman);
	if ( logicalIndex < 0 || logicalIndex >= static_cast< int >(m_logicalMonsters.size()) ) return;

	const LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

	if ( logical.kind != ELogicalMonsterKind::BowMan ) return;
	if ( logical.boundObject != bowman ) return;
	if ( !logical.active ) return;
	if ( logical.dead || logical.hp <= 0 ) return;

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

	for ( int slot = 0; slot < 4; ++slot )
	{
		CGameObject* player = GetPlayerBySlot(slot);

		bool isBowLoad = false;
		bool isBowRelease = false;
		bool hasBowEquipped = false;

		if ( player )
		{
			if ( auto* equip = player->GetComponent<CPlayerEquipmentComponent>() )
			{
				hasBowEquipped = ( equip->GetEquippedWeapon() == EWeaponType::Bow );
			}

			if ( auto* animComp = player->GetComponent<CAnimatorComponent>() )
			{
				if ( auto* ctrl = animComp->EnsureController() )
				{
					isBowLoad = ctrl->IsBowLoadPhase();
					isBowRelease = ctrl->IsBowReleasePhase();
				}
			}
			else if ( auto* ctrl = player->GetAnimController() )
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
				equip->RequestBowReleaseSfxFromLoadPhase();
			}
		}

		if ( hasBowEquipped && isBowRelease && !m_prevBowReleasePhase[slotIndex] )
		{
			RequestReleasePreparedArrow(player, kArrowSpeed, kArrowLife);
		}

		if ( ( !hasBowEquipped || ( !isBowLoad && !isBowRelease ) ) && m_preparedPlayerArrows[slotIndex] )
		{
			if ( auto* arrow = m_preparedPlayerArrows[slotIndex]->GetComponent<CArrowComponent>() )
				arrow->Deactivate();

			m_preparedPlayerArrows[slotIndex] = nullptr;
		}

		m_prevBowLoadPhase[slotIndex] = isBowLoad;
		m_prevBowReleasePhase[slotIndex] = isBowRelease;
	}

	auto ClearPreparedBowmanArrowByIndex = [ this ] (size_t index)
		{
			if ( index < m_preparedBowmanArrows.size() )
			{
				if ( m_preparedBowmanArrows[index] )
				{
					if ( auto* arrow = m_preparedBowmanArrows[index]->GetComponent<CArrowComponent>() )
						arrow->Deactivate();

					m_preparedBowmanArrows[index] = nullptr;
				}
			}

			if ( index < m_prevEnemyBowReleasePhase.size() )
				m_prevEnemyBowReleasePhase[index] = false;
		};

	for ( size_t i = 0; i < m_bowManRefs.size(); ++i )
	{
		CGameObject* bowman = m_bowManRefs[i];

		bool validBoundBowman = false;

		if ( bowman && bowman->GetActive() )
		{
			const int logicalIndex = FindLogicalMonsterIndexByObject(bowman);

			if ( logicalIndex >= 0 && logicalIndex < static_cast< int >(m_logicalMonsters.size()) )
			{
				const LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

				validBoundBowman =
					logical.kind == ELogicalMonsterKind::BowMan &&
					logical.boundObject == bowman &&
					logical.active &&
					!logical.dead &&
					logical.hp > 0;
			}
		}

		if ( !validBoundBowman || IsMonsterDead(bowman) )
		{
			ClearPreparedBowmanArrowByIndex(i);
			continue;
		}

		bool isBowLoad = false;
		bool isBowRelease = false;

		if ( auto* animComp = bowman->GetComponent<CAnimatorComponent>() )
		{
			if ( auto* ctrl = animComp->EnsureMonsterController() )
			{
				isBowLoad = ctrl->IsAttackPrimaryPhase();
				isBowRelease = ctrl->IsAttackChainPhase();
			}
		}

		if ( isBowLoad )
		{
			if ( i < m_preparedBowmanArrows.size() && m_preparedBowmanArrows[i] == nullptr )
				RequestPrepareBowmanArrow(bowman, kEnemyArrowPullBackDistance);
		}

		if ( i < m_prevEnemyBowReleasePhase.size() )
		{
			if ( isBowRelease && !m_prevEnemyBowReleasePhase[i] )
				RequestReleasePreparedBowmanArrow(bowman, kEnemyArrowSpeed, kEnemyArrowLife);
		}

		if ( !isBowLoad && !isBowRelease && i < m_preparedBowmanArrows.size() && m_preparedBowmanArrows[i] )
		{
			if ( auto* arrow = m_preparedBowmanArrows[i]->GetComponent<CArrowComponent>() )
				arrow->Deactivate();

			m_preparedBowmanArrows[i] = nullptr;
		}

		if ( i < m_prevEnemyBowReleasePhase.size() )
			m_prevEnemyBowReleasePhase[i] = isBowRelease;
	}
}

CGameObject* CGameScene::GetPlayerBySlot(int slot) const
{
    if (slot < 0 || slot > 3) return nullptr;
    return m_playersBySlot[(size_t)slot];
}

bool CGameScene::IsPlayerInsideMegaGridCenter(const CGameObject* player) const
{
	if ( !m_sceneGrid.IsInitialized() )
		return false;

	if ( !player )
		return false;

	const XMFLOAT3 pos = player->GetPosition();

	int cellX = -1;
	int cellZ = -1;

	if ( !m_sceneGrid.WorldToCell(pos.x, pos.z, cellX, cellZ) )
		return false;

	int megaX = -1;
	int megaZ = -1;

	if ( !m_sceneGrid.FineCellToMegaGridCell(cellX, cellZ, megaX, megaZ) )
		return false;

	if ( megaX == kCastleCenterMegaGridX && megaZ == kCastleCenterMegaGridZ )
		return IsWorldPositionInsideMegaGrid5CenterSquare250(pos.x, pos.z);

	return m_sceneGrid.IsFineCellInsideMegaGridApproachZone(megaX, megaZ, cellX, cellZ);
}

bool CGameScene::IsPlayerInsideBossStageBattleArea(const CGameObject* player) const
{
	if ( !m_sceneGrid.IsInitialized() )
		return false;

	if ( !player )
		return false;

	const XMFLOAT3 pos = player->GetPosition();

	int cellX = -1;
	int cellZ = -1;

	if ( !m_sceneGrid.WorldToCell(pos.x, pos.z, cellX, cellZ) )
		return false;

	int megaX = -1;
	int megaZ = -1;

	if ( !m_sceneGrid.FineCellToMegaGridCell(cellX, cellZ, megaX, megaZ) )
		return false;

	if ( megaX != kCastleCenterMegaGridX || megaZ != kCastleCenterMegaGridZ )
		return false;

	return IsWorldPositionInsideMegaGrid5CenterSquare250(pos.x, pos.z);
}

bool CGameScene::IsLocalPlayerInsideMegaGridCenter() const
{
	CGameObject* localPlayer = GetPlayer();

	if ( !localPlayer )
		localPlayer = GetPlayerBySlot(0);

	return IsPlayerInsideMegaGridCenter(localPlayer);
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

int CGameScene::ComputePlayerWeaponDamageTierIndexFromClearedMegaGrids() const
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

	return std::clamp(
		clearedCount,
		0,
		kPlayerWeaponDamageMaxTierIndex
	);
}

int CGameScene::GetPlayerSwordAttackPower(int playerSlot) const
{
	const int tier = std::clamp(m_playerWeaponDamageTierIndex, 0, kPlayerWeaponDamageMaxTierIndex);
	const int baseAttackPower = kAttackPowerPlayerSwordByTier[static_cast< size_t >( tier )];
	return ApplyPlayerAttackPowerPotionMultiplier(playerSlot, baseAttackPower);
}

int CGameScene::GetPlayerAxeAttackPower(int playerSlot) const
{
	const int tier = std::clamp(m_playerWeaponDamageTierIndex, 0, kPlayerWeaponDamageMaxTierIndex);
	const int baseAttackPower = kAttackPowerPlayerAxeByTier[static_cast< size_t >( tier )];
	return ApplyPlayerAttackPowerPotionMultiplier(playerSlot, baseAttackPower);
}

int CGameScene::GetPlayerArrowAttackPower(int playerSlot) const
{
	const int tier = std::clamp(m_playerWeaponDamageTierIndex, 0, kPlayerWeaponDamageMaxTierIndex);
	const int baseAttackPower = kAttackPowerPlayerArrowByTier[static_cast< size_t >( tier )];
	return ApplyPlayerAttackPowerPotionMultiplier(playerSlot, baseAttackPower);
}

int CGameScene::GetPlayerBulletAttackPower(int playerSlot) const
{
	const int tier = std::clamp(m_playerWeaponDamageTierIndex, 0, kPlayerWeaponDamageMaxTierIndex);
	const int baseAttackPower = kAttackPowerPlayerBulletByTier[static_cast< size_t >( tier )];
	return ApplyPlayerAttackPowerPotionMultiplier(playerSlot, baseAttackPower);
}

void CGameScene::RefreshPlayerWeaponAttackPowersForSlot(int playerSlot)
{
	if ( playerSlot < 0 || playerSlot >= 4 )
		return;

	const size_t index = static_cast< size_t >(playerSlot);

	if ( index < m_PlayerSwordRefs.size() )
		SetObjectAttackPower(m_PlayerSwordRefs[index], GetPlayerSwordAttackPower(playerSlot));

	if ( index < m_PlayerAxeRefs.size() )
		SetObjectAttackPower(m_PlayerAxeRefs[index], GetPlayerAxeAttackPower(playerSlot));

	if ( index < m_preparedPlayerArrows.size() && m_preparedPlayerArrows[index] )
		SetObjectAttackPower(m_preparedPlayerArrows[index], GetPlayerArrowAttackPower(playerSlot));
}

void CGameScene::RefreshPlayerWeaponAttackPowers()
{
	for ( int slot = 0; slot < 4; ++slot )
		RefreshPlayerWeaponAttackPowersForSlot(slot);
}

void CGameScene::RefreshPlayerWeaponEffectVisuals()
{
	const int visualLevelIndex = GetPlayerWeaponEffectLevelIndex();

	for ( SwordTrailEntry& trail : m_swordTrailEffect.entries )
	{
		if ( !trail.active )
			continue;

		switch ( trail.kind )
		{
		case EWeaponTrailKind::Sword:
		{
			const PlayerMeleeTrailVisualDesc& visual = m_playerSwordTrailVisualDescs[static_cast< size_t >( visualLevelIndex )];
			trail.rootLocal = visual.rootLocal;
			trail.tipLocal = visual.tipLocal;
			trail.widthScale = visual.widthScale;
			trail.color = visual.color;
			trail.alphaScale = visual.alphaScale;
		}
		break;

		case EWeaponTrailKind::Axe:
		{
			const PlayerMeleeTrailVisualDesc& visual = m_playerAxeTrailVisualDescs[static_cast< size_t >( visualLevelIndex )];
			trail.rootLocal = visual.rootLocal;
			trail.tipLocal = visual.tipLocal;
			trail.widthScale = visual.widthScale;
			trail.color = visual.color;
			trail.alphaScale = visual.alphaScale;
		}
		break;

		default:
			break;
		}
	}

	for ( ArrowTrailEntry& trail : m_arrowTrailEffect.entries )
	{
		if ( !trail.active )
			continue;

		const PlayerArrowTrailVisualDesc& visual = m_playerArrowTrailVisualDescs[static_cast< size_t >( visualLevelIndex )];
		trail.sampleLifetimeSec = visual.sampleLifetimeSec;
		trail.halfWidth = visual.halfWidth;
		trail.color = visual.color;
		trail.tailAlpha = visual.tailAlpha;
		trail.headAlpha = visual.headAlpha;
		trail.alphaScale = visual.alphaScale;
	}
}

void CGameScene::RefreshPlayerWeaponDamageTierFromClearedMegaGrids()
{
	const int oldTier = m_playerWeaponDamageTierIndex;
	const int newTier = ComputePlayerWeaponDamageTierIndexFromClearedMegaGrids();

	if ( newTier == oldTier )
		return;

	m_playerWeaponDamageTierIndex = newTier;

	RefreshPlayerWeaponAttackPowers();
	RefreshPlayerWeaponEffectVisuals();

	if ( newTier > oldTier )
	{
		SpawnWeaponLevelUpFireworks();

		if ( m_pAudioManager ) m_pAudioManager->PlaySound2D("Assets/Audio/LevelUp.wav", false, false, 0.5f, false);
	}
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
	RefreshPlayerWeaponDamageTierFromClearedMegaGrids();
}

bool CGameScene::AreAllMonstersInMegaGridDead(int megaGridNumber) const
{
	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return false;

	bool hasRelevantMonster = false;

	for ( const LogicalMonsterState& logical : m_logicalMonsters )
	{
		if ( logical.megaGridNumber != megaGridNumber )
			continue;

#ifdef USING_NETWORK
		hasRelevantMonster = true;

		if ( !logical.dead && logical.hp > 0 )
			return false;
#else
		if ( !logical.active && !logical.dead )
			continue;

		hasRelevantMonster = true;

		if ( logical.active && !logical.dead && logical.hp > 0 )
			return false;
#endif
	}

	return hasRelevantMonster;
}

bool CGameScene::IsBossMonsterObject(const CGameObject* monster) const
{
	if ( !monster )
		return false;

	return std::find(
		m_bossRefs.begin(),
		m_bossRefs.end(),
		monster
	) != m_bossRefs.end();
}

bool CGameScene::IsEnemySpawnerMonsterObject(const CGameObject* monster) const
{
	if ( !monster )
		return false;

	const int logicalIndex = FindLogicalMonsterIndexByObject(monster);
	if ( logicalIndex >= 0 && logicalIndex < static_cast< int >(m_logicalMonsters.size()) )
		return m_logicalMonsters[static_cast< size_t >(logicalIndex)].spawnerEntry;

	return false;
}

bool CGameScene::AreAllPreBossMonstersInMegaGridDead(int megaGridNumber) const
{
	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return false;

	bool hasPreBossMonster = false;

	for ( const LogicalMonsterState& logical : m_logicalMonsters )
	{
		if ( logical.megaGridNumber != megaGridNumber )
			continue;

		if ( logical.kind == ELogicalMonsterKind::Boss )
			continue;

		if ( logical.spawnerEntry )
			continue;

		if ( !logical.active && !logical.dead )
			continue;

		hasPreBossMonster = true;

		if ( logical.active && !logical.dead && logical.hp > 0 )
			return false;
	}

	return hasPreBossMonster;
}

void CGameScene::DamagePreBossMonstersInMegaGrid(int megaGridNumber, int damage)
{
#ifndef USING_NETWORK
	if ( damage <= 0 )
		return;

	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return;

	int damagedCount = 0;

	for ( int logicalIndex = 0; logicalIndex < static_cast< int >(m_logicalMonsters.size()); ++logicalIndex )
	{
		LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

		if ( logical.megaGridNumber != megaGridNumber )
			continue;

		if ( logical.kind == ELogicalMonsterKind::Boss )
			continue;

		if ( logical.spawnerEntry )
			continue;

		if ( !logical.active )
			continue;

		if ( logical.dead || logical.hp <= 0 )
			continue;

		CGameObject* monster = logical.boundObject;

		if ( monster )
		{
			if ( IsMonsterDead(monster) )
			{
				logical.hp = 0;
				logical.dead = true;
				continue;
			}

			auto* hp = monster->GetComponent<CHealthComponent>();
			if ( hp )
			{
				hp->TakeDamage(damage);
				logical.hp = hp->GetCurrentHp();
				logical.maxHp = hp->GetMaxHp();

				hp->RequestHitSfx();
				SpawnBloodSplash(monster, nullptr, nullptr);

				if ( hp->IsDead() )
					BeginMonsterDeath(monster);
			}
			else
			{
				logical.hp = std::max(0, logical.hp - damage);
				if ( logical.hp <= 0 )
				{
					logical.dead = true;
					logical.active = true;
				}
			}
		}
		else
		{
			logical.hp = std::max(0, logical.hp - damage);
			if ( logical.hp <= 0 )
			{
				logical.dead = true;
				logical.active = true;

				if ( logical.keyTrigger )
				{
					const int keyMegaGridNumber = logical.keyTriggerMegaGridNumber;
					if ( keyMegaGridNumber == 6 || keyMegaGridNumber == 8 )
						UnlockKeyBillboardForMegaGrid(keyMegaGridNumber);

					logical.keyTrigger = false;
					logical.keyTriggerMegaGridNumber = -1;
				}
			}
		}

		++damagedCount;
	}

	UpdateMegaGridClearStateFromMonsterDeaths();

	char buf[256];
	sprintf_s(buf, "[BossStageTest] DamagePreBossMonstersInMegaGrid mega=%d damage=%d damaged=%d allDead=%d\n", megaGridNumber, damage, damagedCount, AreAllPreBossMonstersInMegaGridDead(megaGridNumber) ? 1 : 0);
#else
	UNREFERENCED_PARAMETER(megaGridNumber);
	UNREFERENCED_PARAMETER(damage);
#endif
}

void CGameScene::SetBossStageBossAIEnabled(CGameObject* boss, bool enabled)
{
	if ( !boss )
		return;

	if ( auto* bossAI = boss->GetComponent<CBossAIComponent>() )
	{
		const bool bossCombatAIEnabled =
			enabled && m_bSimulateLocalBossAI;

		const bool bossSummonEnabled =
			enabled &&
			(
				m_bSimulateLocalBossSummon ||
				m_bSimulateLocalBossAI
			);

		const bool effectiveAIEnabled =
			bossCombatAIEnabled || bossSummonEnabled;

		bossAI->ConfigureBossSimulation(
			bossCombatAIEnabled,
			bossSummonEnabled
		);

		bossAI->SetEnabledAI(effectiveAIEnabled);

		if ( !bossCombatAIEnabled )
		{
			bossAI->ClearTarget();
			bossAI->ClearPath();
		}

		return;
	}

	auto Configure =
		[ enabled ] (CMonsterAIComponent* ai) -> bool
		{
			if ( !ai )
				return false;

			ai->SetEnabledAI(enabled);

			if ( !enabled )
			{
				ai->ClearTarget();
				ai->ClearPath();
			}

			return true;
		};

	Configure(boss->GetComponent<CMonsterAIComponent>());
}

void CGameScene::RegisterBossStageBossOriginalPosition(CGameObject* boss, const XMFLOAT3& originalPosition)
{
	if ( !boss )
		return;

	BossStageBossPositionState& state = m_bossStageBossPositionStates[boss];

	state.originalPosition = AlignPositionYToTerrainGround(originalPosition, 0.0f);
	state.restoreFramesRemaining = 0;
	state.pendingRestore = false;

	state.renderAllowed = false;
	state.waitAppearBeforeRender = false;
	state.appearPhaseSeen = false;
	state.appearFinished = false;
}

void CGameScene::MoveBossStageBossToHiddenPosition(CGameObject* boss)
{
	if ( !boss )
		return;

	auto it = m_bossStageBossPositionStates.find(boss);

	if ( it == m_bossStageBossPositionStates.end() )
	{
		RegisterBossStageBossOriginalPosition(boss, boss->GetPosition());
		it = m_bossStageBossPositionStates.find(boss);
	}

	if ( it == m_bossStageBossPositionStates.end() )
		return;

	if ( auto* terrainAttach = boss->GetComponent<CTerrainAttachComponent>() )
	{
		terrainAttach->SetHeightOffset(kBossStageBossHiddenYOffset);
		terrainAttach->SnapToTerrain();
	}
	else
	{
		XMFLOAT3 hiddenPosition = it->second.originalPosition;
		hiddenPosition.y += kBossStageBossHiddenYOffset;
		boss->SetPosition(hiddenPosition);
	}

	if ( auto* collider = boss->GetComponent<CColliderComponent>() )
	{
		collider->SetEnabled(false);
		collider->UpdateWorldBounds();
	}
}

void CGameScene::ScheduleBossStageBossPositionRestore(
	CGameObject* boss,
	int delayFrames)
{
	if ( !boss )
		return;

	auto it = m_bossStageBossPositionStates.find(boss);

	if ( it == m_bossStageBossPositionStates.end() )
	{
		// 원칙적으로는 BuildSkinnedBatch에서 이미 등록되어 있어야 한다.
		// 그래도 누락됐을 경우 현재 위치를 원래 위치로 저장한다.
		RegisterBossStageBossOriginalPosition(boss, boss->GetPosition());
		it = m_bossStageBossPositionStates.find(boss);
	}

	if ( it == m_bossStageBossPositionStates.end() )
		return;

	it->second.restoreFramesRemaining = std::max(0, delayFrames);
	it->second.pendingRestore = true;
}

void CGameScene::UpdateBossStageBossPositionRestores()
{
	for ( auto& kv : m_bossStageBossPositionStates )
	{
		CGameObject* boss = kv.first;
		BossStageBossPositionState& state = kv.second;

		if ( !boss )
			continue;

		if ( !state.pendingRestore )
			continue;

		if ( state.restoreFramesRemaining > 0 )
			--state.restoreFramesRemaining;

		if ( state.restoreFramesRemaining > 0 )
			continue;

		if ( auto* terrainAttach = boss->GetComponent<CTerrainAttachComponent>() )
		{
			terrainAttach->SetHeightOffset(0.0f);
			terrainAttach->SnapToTerrain();
		}
		else
		{
			boss->SetPosition(state.originalPosition);
		}

		if ( auto* collider = boss->GetComponent<CColliderComponent>() )
		{
			collider->SetEnabled(true);
			collider->UpdateWorldBounds();
		}

		SetBossStageBossAIEnabled(boss, true);

		state.pendingRestore = false;
	}
}

bool CGameScene::IsBossStageBossRenderAllowed(const CGameObject* boss) const
{
	if ( !boss )
		return false;

	if ( !IsBossMonsterObject(boss) )
		return true;

	const auto it =
		m_bossStageBossPositionStates.find(
			const_cast< CGameObject* >( boss )
		);

	if ( it == m_bossStageBossPositionStates.end() )
		return false;

	return it->second.renderAllowed;
}

void CGameScene::SetBossStageBossRenderAllowed(
	CGameObject* boss,
	bool allowed)
{
	if ( !boss )
		return;

	auto it = m_bossStageBossPositionStates.find(boss);

	if ( it == m_bossStageBossPositionStates.end() )
	{
		RegisterBossStageBossOriginalPosition(boss, boss->GetPosition());
		it = m_bossStageBossPositionStates.find(boss);
	}

	if ( it == m_bossStageBossPositionStates.end() )
		return;

	it->second.renderAllowed = allowed;
}

void CGameScene::UpdateBossStageBossRenderGate()
{
	for ( auto& kv : m_bossStageBossPositionStates )
	{
		CGameObject* boss = kv.first;
		BossStageBossPositionState& state = kv.second;

		if ( !boss )
			continue;

		if ( !boss->GetActive() )
			continue;

		auto* renderer = boss->GetComponent<CSkinnedMeshRendererComponent>();
		auto* animComp = boss->GetComponent<CAnimatorComponent>();

		if ( !animComp )
			continue;

		auto* ctrl = animComp->EnsureMonsterController();

		if ( !ctrl )
			continue;

		const bool isAppearPhase = ctrl->IsAppearPhase();

		if ( state.waitAppearBeforeRender && !state.renderAllowed )
		{
			if ( renderer )
				renderer->SetEnabled(false);
		}

		if ( isAppearPhase )
		{
			state.appearPhaseSeen = true;

			if ( state.waitAppearBeforeRender || !state.renderAllowed )
			{
				state.renderAllowed = true;
				state.waitAppearBeforeRender = false;

				if ( renderer )
					renderer->SetEnabled(true);
			}

			continue;
		}

		if ( state.appearPhaseSeen && !ctrl->IsBusy() && !ctrl->HasPendingCommand() )
		{
			state.appearFinished = true;
		}
	}
}

void CGameScene::SetBossStageBossActive(
	CGameObject* boss,
	bool active,
	bool playAppear)
{
	if ( !boss )
		return;

	auto* renderer = boss->GetComponent<CSkinnedMeshRendererComponent>();
	auto* animator = boss->GetComponent<CAnimatorComponent>();
	auto* collider = boss->GetComponent<CColliderComponent>();
	auto* weaponHitbox = boss->GetComponent<CMonsterWeaponHitboxComponent>();

	if ( !active )
	{
		if ( renderer )
			renderer->SetEnabled(false);

		if ( weaponHitbox )
			weaponHitbox->SetEnabled(false);

		if ( collider )
			collider->SetEnabled(false);

		SetBossStageBossAIEnabled(boss, false);

		if ( animator )
			animator->SetPoseEvaluationEnabled(false);

		{
			auto it = m_bossStageBossPositionStates.find(boss);

			if ( it != m_bossStageBossPositionStates.end() )
			{
				it->second.renderAllowed = false;
				it->second.waitAppearBeforeRender = false;
				it->second.appearPhaseSeen = false;
				it->second.appearFinished = false;
			}
		}

		boss->SetActive(false);
		return;
	}

	const bool useHiddenAppearSpawn = playAppear;

	XMFLOAT3 bossSummonSfxPosition = boss->GetPosition();

	if ( useHiddenAppearSpawn )
	{
		const auto posIt = m_bossStageBossPositionStates.find(boss);

		if ( posIt != m_bossStageBossPositionStates.end() )
			bossSummonSfxPosition = posIt->second.originalPosition;
	}

	// BossSummon.wav는 보스를 y=-100으로 내리기 전에,
	// 반드시 원래 등장 위치 기준으로 1회 재생한다.
	if ( useHiddenAppearSpawn )
		PlayBossSummonSfxAt(bossSummonSfxPosition);

	// Appear 시작 프레임에는 보스를 지하에 둔다.
	// renderer/active 타이밍 문제가 남아 있어도 첫 노출은 y=-100 근처라 화면에 보이지 않는다.
	if ( useHiddenAppearSpawn )
		MoveBossStageBossToHiddenPosition(boss);

	if ( renderer )
		renderer->SetEnabled(false);

	// 원래 위치 복구 전까지 AI는 꺼둔다.
	SetBossStageBossAIEnabled(boss, false);

	boss->SetActive(true);

	if ( animator )
	{
		animator->SetPoseEvaluationEnabled(true);

		if ( playAppear )
		{
			if ( auto* ctrl = animator->EnsureMonsterController() )
			{
				ctrl->RequestCommand(EMonsterAnimCommand::Appear);
				ctrl->Update(0.0f);
			}

			boss->Animate(0.0f);
		}
	}

	// 위치 복구 전까지 collider도 꺼둔다.
	// 지하 위치의 collider가 플레이어나 충돌 시스템에 걸리는 것을 방지한다.
	if ( collider )
	{
		if ( useHiddenAppearSpawn )
		{
			collider->SetEnabled(false);
			collider->UpdateWorldBounds();
		}
		else
		{
			collider->SetEnabled(true);
			collider->UpdateWorldBounds();
		}
	}

	if ( weaponHitbox )
		weaponHitbox->SetEnabled(false);

	if ( auto* hp = boss->GetComponent<CHealthComponent>() )
		hp->ResetToMax();

	if ( auto* bossAI = boss->GetComponent<CBossAIComponent>() )
	{
		bossAI->ResetBossCallState();

		if ( playAppear )
			bossAI->ResetBossOpeningSpellState();
	}

	m_deadMonsters.erase(boss);

	if ( playAppear )
	{
		auto it = m_bossStageBossPositionStates.find(boss);

		if ( it == m_bossStageBossPositionStates.end() )
		{
			RegisterBossStageBossOriginalPosition(boss, boss->GetPosition());
			it = m_bossStageBossPositionStates.find(boss);
		}

		if ( it != m_bossStageBossPositionStates.end() )
		{
			it->second.renderAllowed = false;
			it->second.waitAppearBeforeRender = true;
			it->second.appearPhaseSeen = false;
			it->second.appearFinished = false;
		}

		// Appear phase가 실제로 확인되기 전까지 renderer도 꺼둔다.
		if ( renderer )
			renderer->SetEnabled(false);

		// ctrl->Update(0.0f) 직후 이미 Appear phase가 잡힌 경우에는
		// 같은 프레임 안에서도 render gate를 열 수 있다.
		UpdateBossStageBossRenderGate();
	}
	else
	{
		SetBossStageBossRenderAllowed(boss, true);

		auto it = m_bossStageBossPositionStates.find(boss);

		if ( it != m_bossStageBossPositionStates.end() )
		{
			it->second.appearPhaseSeen = true;
			it->second.appearFinished = true;
		}

		if ( renderer )
			renderer->SetEnabled(true);
	}

	if ( useHiddenAppearSpawn )
	{
		ScheduleBossStageBossPositionRestore(boss, 1);
	}
	else
	{
		SetBossStageBossAIEnabled(boss, true);
	}
}

CGameObject* CGameScene::FindBossStageBossInMegaGrid(int megaGridNumber) const
{
	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return nullptr;

	for ( CGameObject* boss : m_bossRefs )
	{
		if ( !boss )
			continue;

		XMFLOAT3 pos = boss->GetPosition();

		const auto it = m_bossStageBossPositionStates.find(boss);
		if ( it != m_bossStageBossPositionStates.end() )
			pos = it->second.originalPosition;

		const int bossMegaNumber =
			m_sceneGrid.MegaGridNumberFromWorldPosition(pos.x, pos.z);

		if ( bossMegaNumber == megaGridNumber )
			return boss;
	}

	return nullptr;
}

bool CGameScene::TryBeginBossStageSummonSequence()
{
#ifndef USING_NETWORK
	if ( m_bBossStageBossActivated )
		return false;

	if ( m_bBossSummonSequenceStarted )
		return false;

	if ( !AreAllPreBossMonstersInMegaGridDead(5) )
		return false;

	CGameObject* boss = FindBossStageBossInMegaGrid(5);

	if ( !boss )
	{
		return false;
	}

	XMFLOAT3 summonCenter = XMFLOAT3(400.0f, 0.0f, 0.0f);

	const auto it = m_bossStageBossPositionStates.find(boss);
	if ( it != m_bossStageBossPositionStates.end() )
		summonCenter = it->second.originalPosition;
	else
		summonCenter = boss->GetPosition();

	summonCenter = AlignPositionYToTerrainGround(summonCenter, 0.0f);

	m_pendingBossStageBoss = boss;
	m_bBossSummonSequenceStarted = true;
	m_bBossSummonCircleFadeAgeSec = 0.0f;

	m_bBossSummonVisualFadeOutStarted = false;
	m_bBossSummonVisualFadeOutAgeSec = 0.0f;

	SpawnBossSummonVisuals(summonCenter, 0.0f);
	PlayBossSummonCircleSfxAt(summonCenter);

	return true;
#endif

#ifdef USING_NETWORK
	if ( m_serverBossRoomState != 1 ) return false;
	if ( m_bBossStageBossActivated ) return false;
	if ( m_bBossSummonSequenceStarted ) return false;

	CGameObject* boss = FindBossStageBossInMegaGrid(5);
	XMFLOAT3 summonCenter = XMFLOAT3( 0.0f, 0.0f, 400.0f );

	if ( boss )
	{
		const auto it = m_bossStageBossPositionStates.find(boss);
		if ( it != m_bossStageBossPositionStates.end() )
			summonCenter = it->second.originalPosition;
		else
			summonCenter = boss->GetPosition();
	}

	summonCenter = AlignPositionYToTerrainGround(summonCenter, 0.0f);
	m_bossSummonVisualCenter = AlignPositionYToTerrainGround(summonCenter, 0.05f);

	m_pendingBossStageBoss = boss;
	m_bBossSummonSequenceStarted = true;
	m_bBossSummonCircleFadeAgeSec = 0.0f;
	m_bBossSummonVisualFadeOutStarted = false;
	m_bBossSummonVisualFadeOutAgeSec = 0.0f;

	SpawnBossSummonVisuals(summonCenter, 0.0f);
	PlayBossSummonCircleSfxAt(summonCenter);
	return true;
#endif

	return false;
}

bool CGameScene::TryActivateBossStageBoss()
{
#ifndef USING_NETWORK
	if ( m_bBossStageBossActivated )
		return false;

	CGameObject* boss = m_pendingBossStageBoss;

	if ( !boss )
		boss = FindBossStageBossInMegaGrid(5);

	if ( !boss )
	{
		return false;
	}

	XMFLOAT3 shockwaveCenter = XMFLOAT3(400.0f, 0.0f, 0.0f);

	const auto bossPosIt = m_bossStageBossPositionStates.find(boss);
	if ( bossPosIt != m_bossStageBossPositionStates.end() )
		shockwaveCenter = bossPosIt->second.originalPosition;
	else
		shockwaveCenter = boss->GetPosition();

	shockwaveCenter = AlignPositionYToTerrainGround(shockwaveCenter, 0.0f);

	SetBossStageBossActive(boss, true, true);

	SpawnBossShockwave(shockwaveCenter);

	m_bBossStageBossActivated = true;
	m_bBossSummonSequenceStarted = false;
	m_bBossSummonCircleFadeAgeSec = kBossSummonCircleFadeInDurationSec;
	m_pendingBossStageBoss = nullptr;

	StartBossSummonVisualFadeOut();

	return true;
#endif

#ifdef USING_NETWORK
	if ( m_bBossStageBossActivated ) return false;

	CGameObject* boss = m_pendingBossStageBoss;
	if ( !boss )
		boss = FindBossStageBossInMegaGrid(5);
	if ( !boss ) return false;

	XMFLOAT3 shockwaveCenter = XMFLOAT3( 0.0f, 0.0f, 400.0f );
	shockwaveCenter.y = 0.0f;

	SetBossStageBossActive(boss, true, true);
	SpawnBossShockwave(shockwaveCenter);

	m_bBossStageBossActivated = true;
	m_bBossSummonSequenceStarted = false;
	m_bBossSummonCircleFadeAgeSec = kBossSummonCircleFadeInDurationSec;
	m_pendingBossStageBoss = nullptr;

	StartBossSummonVisualFadeOut();
	return true;
#endif

	return false;
}

void CGameScene::UpdateBossStageSummonSequence(float dt)
{
	if ( !m_bBossSummonSequenceStarted )
		return;

	if ( m_bBossStageBossActivated )
		return;

#ifndef USING_NETWORK
	if ( !m_pendingBossStageBoss )
	{
		m_bBossSummonSequenceStarted = false;
		m_bBossSummonCircleFadeAgeSec = 0.0f;
		m_itemBillboardState.bossSummonGlowParticleEmitAccumulatorSec = 0.0f;

		SetBossSummonVisualAlpha(0.0f);
		SetBossSummonVisualActive(false);

		return;
	}
#endif

	if ( dt < 0.0f )
		dt = 0.0f;

	m_bBossSummonCircleFadeAgeSec += dt;

	if ( m_bBossSummonCircleFadeAgeSec > kBossSummonCircleFadeInDurationSec )
		m_bBossSummonCircleFadeAgeSec = kBossSummonCircleFadeInDurationSec;

	const float alpha =
		( kBossSummonCircleFadeInDurationSec > 1.0e-6f )
		? std::clamp(
			m_bBossSummonCircleFadeAgeSec / kBossSummonCircleFadeInDurationSec,
			0.0f,
			1.0f
		)
		: 1.0f;

	SetBossSummonVisualAlpha(alpha);

	bool shouldEmitSummonParticles = false;
	XMFLOAT3 center = XMFLOAT3(0.0f, 0.0f, 0.0f);

	if ( m_pendingBossStageBoss )
	{
		center = m_pendingBossStageBoss->GetPosition();

		const auto it = m_bossStageBossPositionStates.find(m_pendingBossStageBoss);
		if ( it != m_bossStageBossPositionStates.end() )
			center = it->second.originalPosition;

		center = AlignPositionYToTerrainGround(center, 0.05f);
		shouldEmitSummonParticles = true;
	}
#ifdef USING_NETWORK
	else
	{
		center = m_bossSummonVisualCenter;
		shouldEmitSummonParticles = true;
	}
#endif

	if ( shouldEmitSummonParticles )
		EmitMagicCircleGlowParticles(center, 110.0f, alpha, dt, m_itemBillboardState.bossSummonGlowParticleEmitAccumulatorSec, kBossSummonGlowParticleEmitIntervalSec, kBossSummonGlowParticlesPerEmit, kBossSummonGlowParticleIntensityScale);

	if ( alpha < 1.0f )
		return;

#ifndef USING_NETWORK
	TryActivateBossStageBoss();
#endif
}

void CGameScene::StartBossSummonVisualFadeOut()
{
	m_bBossSummonVisualFadeOutStarted = true;
	m_bBossSummonVisualFadeOutAgeSec = 0.0f;

	SetBossSummonVisualActive(true);
	SetBossSummonVisualAlpha(1.0f);
}

void CGameScene::UpdateBossSummonVisualFadeOut(float dt)
{
	if ( !m_bBossSummonVisualFadeOutStarted )
		return;

	if ( dt < 0.0f )
		dt = 0.0f;

	m_bBossSummonVisualFadeOutAgeSec += dt;

	const float t =
		( kBossSummonCircleFadeOutDurationSec > 1.0e-6f )
		? std::clamp(
			m_bBossSummonVisualFadeOutAgeSec / kBossSummonCircleFadeOutDurationSec,
			0.0f,
			1.0f
		)
		: 1.0f;

	const float alpha = 1.0f - t;

	SetBossSummonVisualAlpha(alpha);

	if ( alpha > 0.001f )
	{
#ifdef USING_NETWORK
		XMFLOAT3 center = m_bossSummonVisualCenter;
#else
		XMFLOAT3 center = XMFLOAT3(400.0f, 0.0f, 400.0f);
#endif

		if ( m_pendingBossStageBoss )
		{
			center = m_pendingBossStageBoss->GetPosition();
		}
		else if ( !m_bossRefs.empty() && m_bossRefs[0] )
		{
			center = m_bossRefs[0]->GetPosition();

			const auto it = m_bossStageBossPositionStates.find(m_bossRefs[0]);
			if ( it != m_bossStageBossPositionStates.end() )
				center = it->second.originalPosition;
		}

		center = AlignPositionYToTerrainGround(center, 0.05f);

		EmitMagicCircleGlowParticles(center, 110.0f, alpha, dt, m_itemBillboardState.bossSummonGlowParticleEmitAccumulatorSec, kBossSummonGlowParticleEmitIntervalSec, kBossSummonGlowParticlesPerEmit, kBossSummonGlowParticleIntensityScale);
	}

	if ( t < 1.0f )
		return;

	SetBossSummonVisualAlpha(0.0f);
	SetBossSummonVisualActive(false);

	m_bBossSummonVisualFadeOutStarted = false;
	m_bBossSummonVisualFadeOutAgeSec = 0.0f;
	m_itemBillboardState.bossSummonGlowParticleEmitAccumulatorSec = 0.0f;
}

void CGameScene::RegisterMutantKeyTriggerIfNeeded(CGameObject* mutant, int megaGridNumber)
{
	if ( !mutant )
		return;

	if ( megaGridNumber != 6 && megaGridNumber != 8 )
		return;

	const int logicalIndex = FindLogicalMonsterIndexByObject(mutant);
	if ( logicalIndex < 0 || logicalIndex >= static_cast< int >(m_logicalMonsters.size()) )
		return;

	LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

	if ( logical.kind != ELogicalMonsterKind::Mutant )
		return;

	if ( logical.spawnerEntry )
		return;

	if ( m_mutantKeyTriggerRegisteredByMega[static_cast< size_t >( megaGridNumber )] && !logical.keyTrigger )
		return;

	logical.keyTrigger = true;
	logical.keyTriggerMegaGridNumber = megaGridNumber;
	m_mutantKeyTriggerRegisteredByMega[static_cast< size_t >( megaGridNumber )] = true;
}

void CGameScene::UnlockKeyBillboardForMegaGrid(int megaGridNumber)
{
	if ( megaGridNumber != 6 && megaGridNumber != 8 )
		return;

	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
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

		return;
	}
}

void CGameScene::HandleMutantKeyTriggerDeath(CGameObject* monster)
{
	if ( !monster )
		return;

	const int logicalIndex = FindLogicalMonsterIndexByObject(monster);
	if ( logicalIndex < 0 || logicalIndex >= static_cast< int >(m_logicalMonsters.size()) )
		return;

	LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

	if ( !logical.keyTrigger )
		return;

	const int megaGridNumber = logical.keyTriggerMegaGridNumber;
	if ( megaGridNumber != 6 && megaGridNumber != 8 )
		return;

	UnlockKeyBillboardForMegaGrid(megaGridNumber);

	logical.keyTrigger = false;
	logical.keyTriggerMegaGridNumber = -1;
}

void CGameScene::UpdateMegaGridClearStateFromMonsterDeaths()
{
	// 2번 메가그리드: 해당 메가그리드의 모든 몬스터 사망 시 클리어.
	if ( !m_sceneGrid.IsMegaGridCleared(1, 0) )
	{
		if ( AreAllMonstersInMegaGridDead(2) )
			MarkMegaGridClearedByNumber(2);
	}

	// 5번 메가그리드: 보스 등장 전 기본 배치 몬스터가 모두 죽으면
	// 보스를 바로 활성화하지 않고, 소환 마법진 fade-in부터 시작한다.
	TryBeginBossStageSummonSequence();

	// 5번 메가그리드: 최종 클리어는 logical boss 사망 시 처리.
	if ( !m_sceneGrid.IsMegaGridCleared(1, 1) )
	{
		for ( const LogicalMonsterState& logical : m_logicalMonsters )
		{
			if ( logical.kind != ELogicalMonsterKind::Boss )
				continue;

			if ( logical.megaGridNumber != 5 )
				continue;

			if ( logical.active && ( logical.dead || logical.hp <= 0 ) )
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

	for ( size_t i = 0; i < m_ghoulRefs.size(); ++i )
	{
		if ( m_ghoulRefs[i] != monster )
			continue;

		if ( i < m_prevGhoulAttackPhase.size() )
			m_prevGhoulAttackPhase[i] = false;

		break;
	}

	const int bowmanIndex = GetBowManIndexFromObject(monster);
	if ( bowmanIndex >= 0 )
	{
		const size_t idx = static_cast< size_t >(bowmanIndex);

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

		if ( idx < m_prevBowManSfxLoadPhase.size() )
			m_prevBowManSfxLoadPhase[idx] = false;
	}

	const int swordManIndex = GetSwordManIndexFromObject(monster);
	if ( swordManIndex >= 0 )
	{
		const size_t idx = static_cast< size_t >(swordManIndex);

		if ( idx < m_prevSwordManAttackPhase.size() )
			m_prevSwordManAttackPhase[idx] = false;

		if ( idx < m_EnemySwordRefs.size() )
		{
			CGameObject* sword = m_EnemySwordRefs[idx];

			if ( sword )
			{
				if ( auto* collider = sword->GetComponent<CColliderComponent>() )
					collider->SetEnabled(false);

				if ( auto* hitbox = sword->GetComponent<CMonsterWeaponHitboxComponent>() )
					hitbox->SetEnabled(false);
			}
		}
	}

	for ( size_t i = 0; i < m_MutantRefs.size(); ++i )
	{
		if ( m_MutantRefs[i] != monster )
			continue;

		if ( i < m_prevMutantAttackPhase.size() )
			m_prevMutantAttackPhase[i] = false;

		break;
	}

	if ( auto* ownerWeaponHitbox = monster->GetComponent<CMonsterWeaponHitboxComponent>() )
		ownerWeaponHitbox->SetEnabled(false);
}

void CGameScene::DisableAllMonsterAIComponents(CGameObject* monster) const
{
	if ( !monster )
		return;

	float yawDeg = 0.0f;
	if ( auto* tr = monster->GetComponent<CTransformComponent>() )
		yawDeg = QuaternionToYawDegrees(tr->rotation);

	const XMFLOAT3 homePosition = monster->GetPosition();

	auto DisableAI = [ homePosition, yawDeg ] (CMonsterAIComponent* ai)
		{
			if ( !ai )
				return;

			ai->SetEnabledAI(false);
			ai->ResetRuntimeStateForReuse(homePosition, yawDeg);
		};

	DisableAI(monster->GetComponent<CGhoulAIComponent>());
	DisableAI(monster->GetComponent<CEnemySpawnerGhoulAIComponent>());
	DisableAI(monster->GetComponent<CBossStageMonsterAIComponent>());
	DisableAI(monster->GetComponent<CSwordManAIComponent>());
	DisableAI(monster->GetComponent<CBowManAIComponent>());
	DisableAI(monster->GetComponent<CMutantAIComponent>());
	DisableAI(monster->GetComponent<CBossAIComponent>());
	DisableAI(monster->GetComponent<CMonsterAIComponent>());
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
	const int logicalIndex = FindLogicalMonsterIndexByObject(monster);
	if ( logicalIndex >= 0 && logicalIndex < static_cast< int >(m_logicalMonsters.size()) )
	{
		LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];
		logical.position = monster->GetPosition();
		logical.hp = 0;
		logical.dead = true;
		logical.active = true;

		if ( auto* tr = monster->GetComponent<CTransformComponent>() )
			logical.yawDeg = QuaternionToYawDegrees(tr->rotation);
	}

	if ( IsBossMonsterObject(monster) )
	{
		PlayBossDeathSfxAt(monster->GetPosition());
		BeginBossDeathEffect(monster);
	}

	const bool isMutantKeyTrigger =
		m_mutantKeyTriggerMegaByObject.find(monster) !=
		m_mutantKeyTriggerMegaByObject.end();

	HandleMutantKeyTriggerDeath(monster);

	CancelMonsterPreparedActions(monster);

	// 열쇠 트리거 뮤턴트는 사망 위치에 열쇠가 열리므로 바디 콜라이더를 즉시 제거한다.
	if ( auto* collider = monster->GetComponent<CColliderComponent>() )
		collider->DisableCollisionAndKeepUpdatingForSeconds(isMutantKeyTrigger ? 0.0f : 5.0f);

	DisableAllMonsterAIComponents(monster);

	if ( auto* animComp = monster->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->EnsureMonsterController() )
		{
			ctrl->PlayDeathFromStart();
			return;
		}
	}
}

void CGameScene::UpdateMonsterDeathStates()
{
#ifdef USING_NETWORK
	return;
#endif

	bool deathStateChanged = false;

	for ( const SkinnedComponentCache& cache : m_skinnedComponentCache )
	{
		CGameObject* obj = cache.object;
		if ( !obj )
			continue;

		if ( !cache.isNpc )
			continue;

		if ( !obj->GetActive() )
			continue;

		const int logicalIndex = FindLogicalMonsterIndexByObject(obj);
		if ( logicalIndex < 0 || logicalIndex >= static_cast< int >(m_logicalMonsters.size()) )
			continue;

		LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

		const bool wasAlreadyDead = ( m_deadMonsters.find(obj) != m_deadMonsters.end() );

		logical.position = obj->GetPosition();
		logical.active = true;

		if ( auto* tr = obj->GetComponent<CTransformComponent>() )
			logical.yawDeg = QuaternionToYawDegrees(tr->rotation);

		if ( !cache.health )
		{
			logical.dead = wasAlreadyDead;

			if ( wasAlreadyDead )
				logical.hp = 0;

			continue;
		}

		logical.maxHp = std::max(1, cache.health->GetMaxHp());
		logical.hp = std::clamp(cache.health->GetCurrentHp(), 0, logical.maxHp);

		const bool healthDead = cache.health->IsDead() || logical.hp <= 0;

		if ( healthDead || wasAlreadyDead )
		{
			logical.hp = 0;
			logical.dead = true;
			logical.active = true;
		}
		else
		{
			logical.dead = false;
			logical.active = true;
		}

		if ( healthDead )
		{
			BeginMonsterDeath(obj);

			if ( !wasAlreadyDead && m_deadMonsters.find(obj) != m_deadMonsters.end() )
				deathStateChanged = true;
		}
	}

	if ( deathStateChanged )
		UpdateMegaGridClearStateFromMonsterDeaths();
}

void CGameScene::BeginLocalPlayerDeath(CGameObject* player)
{
	if ( !player )
		return;

	if ( m_bLocalPlayerDead )
		return;

#ifndef USING_NETWORK
	if ( CInventoryComponent* inventory = player->GetComponent<CInventoryComponent>() )
	{
		inventory->ForceRestoreAllEffects();

		if ( inventory->ConsumeAttackPowerDirty() )
			RefreshPlayerWeaponAttackPowersForSlot(m_localPlayerSlot);

		SyncLocalInventoryToHud();
	}
#endif

	m_bLocalPlayerDead = true;
	m_localPlayerRespawnTimer = 0.0f;

	PlayPlayerDeathSfxAt(player->GetPosition());

	SetLocalPlayerControlEnabled(false);
	CancelLocalPlayerPreparedActions();

	if ( auto* collider = player->GetComponent<CColliderComponent>() )
		collider->DisableCollisionAndKeepUpdatingForSeconds(5.0f);

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
	{
		hp->ResetToMax();
		hp->SetIncomingDamageScale(1.0f);
	}

	if ( auto* collider = player->GetComponent<CColliderComponent>() )
	{
		collider->CancelDeferredDisable();
		collider->SetEnabled(true);
		collider->SetCollisionEnabled(true);
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
		}
#endif
		return false;
	}

#ifdef USING_NETWORK
	// A server teleport can be valid in the authoritative collision set while
	// overlapping a client-side static collider that disagrees with it. Do not
	// trap prediction at that origin; let server-validated input move it out.
	const bool hitWorldStaticAtPreviousPos = TestPositionAgainstWorldStatic(previousPos);
	localPlayer->SetPosition(currentPos);
	collider->UpdateWorldBounds();
	if ( hitWorldStaticAtPreviousPos )
		return false;
#endif

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

	const int slot = GetPlayerSlotFromObject(shooter);
	if ( slot < 0 || slot >= 4 )
		return;

	for ( CGameObject* arrowObj : m_arrowRefs )
    {
        if (!arrowObj) continue;

        auto* arrow = arrowObj->GetComponent<CArrowComponent>();
        if (!arrow) continue;

		if ( arrow->IsActive() ) continue;

		SetObjectAttackPower(arrowObj, GetPlayerArrowAttackPower(slot));

		m_playerWeaponOwnerByObject[arrowObj] = shooter;

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

	const int slot = GetPlayerSlotFromObject(shooter);
	if ( slot < 0 || slot >= 4 )
		return;

	for ( CGameObject* bulletObj : m_bulletRefs )
	{
		if ( !bulletObj ) continue;

		auto* bullet = bulletObj->GetComponent<CBulletComponent>();
		if ( !bullet ) continue;
		if ( bullet->IsActive() ) continue;

		SetObjectAttackPower(bulletObj, GetPlayerBulletAttackPower(slot));

		m_playerWeaponOwnerByObject[bulletObj] = shooter;

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
	if ( !pKeysBuffer )
	{
		m_bPrevLocalMonsterChaseToggleKeyDown = false;
		m_bPrevDebugDamageMegaGrid5KeyDown = false;
		m_bPrevLocalStageTeleportKeyDown.fill(false);
		m_bPrevInventoryUseKeyDown.fill(false);

		return false;
	}

	const bool stageTeleportModifierDown = ( ( pKeysBuffer[VK_LCONTROL] & 0xF0 ) != 0 ) || ( ( pKeysBuffer[VK_RCONTROL] & 0xF0 ) != 0 );

#ifndef USING_NETWORK
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
	// Enter: 테스트용. 5번 메가그리드의 보스/스포너 제외 몬스터에게 1000 대미지.
	// ---------------------------------------------------------------------
	const bool enterDown = ( pKeysBuffer[VK_RETURN] & 0xF0 ) != 0;

	if ( enterDown && !m_bPrevDebugDamageMegaGrid5KeyDown )
	{
		m_bPrevDebugDamageMegaGrid5KeyDown = true;

		DamagePreBossMonstersInMegaGrid(5, 1000);
		return true;
	}

	m_bPrevDebugDamageMegaGrid5KeyDown = enterDown;
#endif

	// ---------------------------------------------------------------------
	// 1~4: 인벤토리 아이템 사용 요청
	// ---------------------------------------------------------------------
	for ( int slot = 0; slot < CGameSceneHUD::kInventorySlotCount; ++slot )
	{
		const bool down = ( pKeysBuffer['1' + slot] & 0xF0 ) != 0;
		const bool prevDown = m_bPrevInventoryUseKeyDown[static_cast< size_t >(slot)];

		if ( down && !prevDown && !stageTeleportModifierDown )
			RequestUseInventoryItemSlot(slot);

		m_bPrevInventoryUseKeyDown[static_cast< size_t >(slot)] = down;
	}

#ifndef USING_NETWORK
	// ---------------------------------------------------------------------
	// Ctrl + 1~9: 로컬 스테이지 메가그리드 강제 텔레포트
	//
	// 배치:
	// 789
	// 456
	// 123
	//
	// 숫자키 단독 입력은 아이템 사용 등에 넘기기 위해 여기서 consume하지 않는다.
	// false이면 입력 상태만 갱신하고 실제 텔레포트는 하지 않는다.
	// ---------------------------------------------------------------------

	for ( int megaGridNumber = 1; megaGridNumber <= CSceneGrid::kMegaGridCount; ++megaGridNumber )
	{
		const bool down = ( pKeysBuffer['0' + megaGridNumber] & 0xF0 ) != 0;
		const bool prevDown = m_bPrevLocalStageTeleportKeyDown[static_cast< size_t >( megaGridNumber )];

		if ( down && !prevDown )
		{
			m_bPrevLocalStageTeleportKeyDown[static_cast< size_t >( megaGridNumber )] = true;

			if ( stageTeleportModifierDown && m_bSimulateLocalStageTeleport )
				return TryTeleportLocalPlayerToMegaGridByNumber(megaGridNumber);

			return false;
		}

		m_bPrevLocalStageTeleportKeyDown[static_cast< size_t >( megaGridNumber )] = down;
	}
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

	const uint64_t ticksElapsed = static_cast<uint64_t>(m_timeSinceLastFramePacket / kServerTickSeconds);
	const uint64_t renderTick = m_lastReceivedServerTick + ticksElapsed - kNetworkInterpolationDelayTicks;

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

float CGameScene::SampleClientTerrainY(float worldX, float worldZ, float fallbackY) const
{
	if ( !m_TerrainData )
		return fallbackY;

	const XMFLOAT3 terrainPos = m_TerrainData->GetWorldPosition();
	const float localX = worldX - terrainPos.x;
	const float localZ = worldZ - terrainPos.z;

	if ( localX < 0.0f || localZ < 0.0f ||
		 localX >= m_TerrainData->GetWorldWidth() ||
		 localZ >= m_TerrainData->GetWorldLength() )
		return fallbackY;

	return terrainPos.y + m_TerrainData->GetHeight(localX, localZ);
}

XMFLOAT3 CGameScene::ResolveNetworkActorY(
	uint64_t actorId,
	bool isPlayer,
	const XMFLOAT3& serverPos,
	float dt)
{
	constexpr float kEnterServerYThreshold = 1.0f;
	constexpr float kExitServerYThreshold = 0.25f;
	constexpr float kServerYHoldSeconds = 0.5f;

	XMFLOAT3 resolved = serverPos;
	const float terrainY = SampleClientTerrainY(serverPos.x, serverPos.z, serverPos.y);
	const float serverDelta = std::fabs(serverPos.y - terrainY);

	auto& states = isPlayer ? m_networkPlayerYStates : m_networkEnemyYStates;
	NetworkActorYState& yState = states[actorId];

	if ( !yState.useServerY )
	{
		if ( serverDelta > kEnterServerYThreshold )
		{
			yState.useServerY = true;
			yState.serverYHoldSec = kServerYHoldSeconds;
		}
	}
	else
	{
		if ( yState.serverYHoldSec > 0.0f )
			yState.serverYHoldSec = std::max(0.0f, yState.serverYHoldSec - dt);
		else if ( serverDelta < kExitServerYThreshold )
			yState.useServerY = false;
	}

	resolved.y = yState.useServerY ? serverPos.y : terrainY;
	return resolved;
}

#ifdef USING_NETWORK
void CGameScene::ApplyNetworkEnemySnapshotToLogicalMonsters(const FrameSnapshot& snapshot, float dt)
{
	std::unordered_set<uint64_t> snapshotEnemyIds;
	snapshotEnemyIds.reserve(snapshot.enemies.size());

	for ( const EnemyState& state : snapshot.enemies )
	{
		if ( state.lifecycleState == static_cast< uint32_t >(Protocol::ENEMY_STATE_SPAWN_PENDING) )
			continue;
		snapshotEnemyIds.insert(state.id);
	}

	for ( int logicalIndex = 0; logicalIndex < static_cast< int >(m_logicalMonsters.size()); ++logicalIndex )
	{
		LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

		if ( logical.serverId == static_cast< uint64_t >( -1 ) )
			continue;

		if ( logical.kind == ELogicalMonsterKind::Boss )
			continue;

		if ( snapshotEnemyIds.find(logical.serverId) != snapshotEnemyIds.end() )
			continue;

		logical.active = false;

		if ( logical.boundObject )
			UnbindActualMonsterFromLogical(logical.boundObject);
	}

	for ( const EnemyState& state : snapshot.enemies )
	{
		if ( state.lifecycleState == static_cast< uint32_t >(Protocol::ENEMY_STATE_SPAWN_PENDING) )
			continue;

		auto logicalIt = m_logicalMonsterIndexByServerId.find(state.id);
		if ( logicalIt == m_logicalMonsterIndexByServerId.end() )
			continue;

		const int logicalIndex = logicalIt->second;
		if ( logicalIndex < 0 || logicalIndex >= static_cast< int >(m_logicalMonsters.size()) )
			continue;

		LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];

		const DecodedAnimStateCode previousDecoded = DecodeStateCode(logical.animationStateCode);
		const bool wasDead = logical.dead || logical.hp <= 0 || previousDecoded.die;

		const DecodedAnimStateCode decoded = DecodeStateCode(state.animation.stateCode);
		const XMFLOAT3 resolvedServerPosition = ResolveNetworkActorY(state.id, false, state.position, dt);

		EnemyDRState& dr = m_enemyDRStates[state.id];
		if ( !dr.initialized )
		{
			dr.predictedPos = resolvedServerPosition;
			dr.initialized = true;
		}
		else
		{
			constexpr float kCorrectionAlpha = 0.35f;
			dr.predictedPos = LerpPosition(dr.predictedPos, resolvedServerPosition, kCorrectionAlpha);
		}

		const float yawRad = XMConvertToRadians(state.yaw);
		dr.moveDir = XMFLOAT3(std::sinf(yawRad), 0.0f, std::cosf(yawRad));
		dr.speed = ( decoded.hasMove && !decoded.die && !decoded.hit ) ? ( decoded.run ? 2.0f : 1.0f ) : 0.0f;

		const int oldMegaGridNumber = logical.megaGridNumber;

		logical.position = dr.predictedPos;
		logical.yawDeg = state.yaw;
		logical.hp = static_cast< int >( state.hp );
		logical.active = true;
		logical.dead = decoded.die || state.hp <= 0;
		logical.animationStateCode = state.animation.stateCode;
		logical.megaGridNumber = m_sceneGrid.MegaGridNumberFromWorldPosition(logical.position.x, logical.position.z);

		UpdateLogicalMonsterMegaGridIndex(logicalIndex, oldMegaGridNumber);

		const bool isNowDead = logical.dead || logical.hp <= 0 || decoded.die;
		if ( isNowDead && !wasDead )
		{
			if ( logical.keyTrigger )
			{
				const int keyMegaGridNumber = logical.keyTriggerMegaGridNumber;
				if ( keyMegaGridNumber == 6 || keyMegaGridNumber == 8 )
					UnlockKeyBillboardForMegaGrid(keyMegaGridNumber);

				logical.keyTrigger = false;
				logical.keyTriggerMegaGridNumber = -1;
			}

			UpdateMegaGridClearStateFromMonsterDeaths();
		}

		if ( !logical.boundObject )
			m_prevEnemyNetworkStateCode[state.id] = state.animation.stateCode;
	}
}

void CGameScene::ApplyNetworkEnemyVisualSnapshot(CGameObject* monster, int logicalMonsterIndex, const EnemyState& state, float dt)
{
	if ( !monster )
		return;

	if ( logicalMonsterIndex < 0 || logicalMonsterIndex >= static_cast< int >(m_logicalMonsters.size()) )
		return;

	LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalMonsterIndex)];

	monster->SetPosition(logical.position.x, logical.position.y, logical.position.z);

	const bool logicalVisible =
		logical.active && !logical.dead && logical.hp > 0;
	const bool bossSnapshotShouldRender =
		logicalVisible && m_serverBossRoomState >= 3;

	if ( IsBossMonsterObject(monster) && bossSnapshotShouldRender )
	{
		if ( !m_bBossStageBossActivated )
		{
			m_pendingBossStageBoss = monster;
			TryActivateBossStageBoss();
		}
	}

	if ( auto* hp = monster->GetComponent<CHealthComponent>() )
		hp->SetCurrentHp(static_cast< int >(state.hp));

	if ( auto* tr = monster->GetComponent<CTransformComponent>() )
		tr->SetYawDegrees(state.yaw);

	if ( auto* collider = monster->GetComponent<CColliderComponent>() )
	{
		const bool bossWaitingForAppearRender =
			IsBossMonsterObject(monster) &&
			m_bBossStageBossActivated &&
			!IsBossStageBossRenderAllowed(monster);
		const bool collisionEnabled =
			logical.active && !logical.dead && logical.hp > 0 &&
			!bossWaitingForAppearRender;
		collider->CancelDeferredDisable();
		collider->SetEnabled(collisionEnabled);
		collider->SetCollisionEnabled(collisionEnabled);
		collider->UpdateWorldBounds();
	}

	if ( logical.dead )
		m_deadMonsters.insert(monster);
	else
		m_deadMonsters.erase(monster);

	if ( auto* animComp = monster->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->EnsureMonsterController() )
		{
			const DecodedAnimStateCode decoded = DecodeStateCode(state.animation.stateCode);

			EMonsterAnimState locomotionState = EMonsterAnimState::Idle;
			if ( decoded.hasMove )
				locomotionState = decoded.run ? EMonsterAnimState::Run : EMonsterAnimState::Move;

			ctrl->SetLocomotionState(locomotionState);

			const uint32_t prevStateCode = ( m_prevEnemyNetworkStateCode.find(state.id) != m_prevEnemyNetworkStateCode.end() ) ? m_prevEnemyNetworkStateCode[state.id] : 0u;
			const DecodedAnimStateCode prevDecoded = DecodeStateCode(prevStateCode);

			if ( decoded.die && !prevDecoded.die )
			{
				logical.position = monster->GetPosition();
				logical.hp = 0;
				logical.dead = true;
				logical.active = true;

				CancelMonsterPreparedActions(monster);
				DisableAllMonsterAIComponents(monster);

				if ( auto* weaponHitbox = monster->GetComponent<CMonsterWeaponHitboxComponent>() )
					weaponHitbox->SetEnabled(false);

				if ( auto* collider = monster->GetComponent<CColliderComponent>() )
					collider->DisableCollisionAndKeepUpdatingForSeconds(5.0f);

				if ( IsBossMonsterObject(monster) )
				{
					PlayBossDeathSfxAt(monster->GetPosition());
					BeginBossDeathEffect(monster);
				}
				else if ( auto* hp = monster->GetComponent<CHealthComponent>() )
				{
					hp->RequestHitSfx();
				}

				HandleMutantKeyTriggerDeath(monster);

				ctrl->PlayDeathFromStart();
				UpdateMegaGridClearStateFromMonsterDeaths();
			}
			else if ( decoded.hit && !prevDecoded.hit )
			{
				ctrl->RequestCommand(EMonsterAnimCommand::Hit);
				SpawnBloodSplash(monster, nullptr, nullptr);

				if ( auto* hp = monster->GetComponent<CHealthComponent>() )
					hp->RequestHitSfx();
			}
			else if ( decoded.attack && !prevDecoded.attack )
			{
				ctrl->RequestCommand(EMonsterAnimCommand::Attack);
			}
			else if ( decoded.bossSpell && !prevDecoded.bossSpell )
			{
				ctrl->RequestCommand(EMonsterAnimCommand::Spell);
			}
			else if ( decoded.bossCall && !prevDecoded.bossCall )
			{
				ctrl->RequestCommand(EMonsterAnimCommand::Call);
			}

			m_prevEnemyNetworkStateCode[state.id] = state.animation.stateCode;
			ctrl->Update(0.0f);
		}
	}

	UNREFERENCED_PARAMETER(dt);
}
#endif

void CGameScene::ApplyNetworkPredictedTerrainY(CGameObject* obj)
{
	if ( !obj )
		return;

	const int slot = m_localPlayerSlot;
	NetworkActorYState& yState = m_networkPlayerYStates[static_cast<uint64_t>(slot)];
	if ( yState.useServerY )
		return;

	XMFLOAT3 pos = obj->GetPosition();
	const float terrainY = SampleClientTerrainY(pos.x, pos.z, pos.y);
	if ( std::fabs(pos.y - terrainY) <= 0.001f )
		return;

	pos.y = terrainY;
	obj->SetPosition(pos);
}
#endif

void CGameScene::AnimateObjects(float dt)
{
	m_fElapsedTime = dt;
	m_waterAccumulatedTime += dt;

	CGameObject* local = GetPlayer();
	if ( !local )
		local = GetPlayerBySlot(0);

#ifndef USING_NETWORK
	ReconcileLogicalMonsterVisualBindings();

	UpdateBossStageBossPositionRestores();
	UpdateBossStageBossRenderGate();

	UpdateBossSummonVisualFadeOut(dt);
	UpdateBossStageSummonSequence(dt);
	UpdateBossShockwave(dt);
	UpdateBossCallSummonCircles(dt);
	UpdateBossCallSummonWwwEffects(dt);

	UpdateEnemySpawnerTimedGhoulWaves(dt);
#else
	UpdateBossStageBossPositionRestores();
	UpdateBossStageBossRenderGate();

	UpdateBossSummonVisualFadeOut(dt);
	UpdateBossStageSummonSequence(dt);
	UpdateBossShockwave(dt);
	UpdateBossCallSummonCircles(dt);
	UpdateBossCallSummonWwwEffects(dt);
#endif

	UpdateBossDeathEffect(dt);
	UpdateMuzzleFlashes(dt);
	UpdateGunSmokes(dt);
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
	ForcedTransformEvent forcedTransformEvent{};
	while (g_NetworkQueue.TryPopForcedTransform(forcedTransformEvent))
	{
		const int slot = static_cast<int>(forcedTransformEvent.playerId);
		if (slot != m_localPlayerSlot)
			continue;

		CGameObject* player = GetPlayerBySlot(slot);
		if (!player)
			continue;

		const float yawDeltaDeg = forcedTransformEvent.yawDelta;

		CCamera* camera = GetMainCamera();
		float targetYaw = 0.0f;
		if (camera)
		{
			targetYaw = NormalizeYawDegrees180(camera->GetYaw() + yawDeltaDeg);
			camera->GetYaw() = targetYaw;

			XMFLOAT3 cameraTarget = player->GetPosition();
			cameraTarget.y += 1.7f;
			camera->Update(cameraTarget, 0.0f);
			camera->SetLookAt(cameraTarget);
			camera->RegenerateViewMatrix();
		}
		else if (auto* tr = player->GetComponent<CTransformComponent>())
		{
			targetYaw = NormalizeYawDegrees180(QuaternionToYawDegrees(tr->rotation) + yawDeltaDeg);
		}
		else
		{
			targetYaw = NormalizeYawDegrees180(yawDeltaDeg);
		}

		if (auto* controller = player->GetComponent<CPlayerControllerComponent>())
		{
			controller->SetYawDegrees(targetYaw);
			controller->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
			controller->SetInputDirection(static_cast<DWORD>(0));
			controller->SetRunRequested(false);
		}
		else if (auto* tr = player->GetComponent<CTransformComponent>())
		{
			tr->SetYawDegrees(targetYaw);
		}
	}

    DequeueNetworkMessage(NetworkMessageType::FrameState);

    if (std::holds_alternative<FrameSnapshot>(m_pendingNetworkMessage.data))
    {
        const FrameSnapshot& receivedSnapshot = std::get<FrameSnapshot>(m_pendingNetworkMessage.data);
		PushNetworkFrameSnapshot(receivedSnapshot);
		m_timeSinceLastFramePacket = 0.0f;
		const FrameSnapshot& latestSnapshot =
			m_frameSnapshotBuffer.empty() ? receivedSnapshot : m_frameSnapshotBuffer.back();
		FrameSnapshot snapshot = BuildInterpolatedFrameSnapshot(latestSnapshot);

		const uint32_t newBossRoomState = receivedSnapshot.bossRoomState;
		if ( newBossRoomState != m_serverBossRoomState )
		{
			const uint32_t prevState = m_serverBossRoomState;
			m_serverBossRoomState = newBossRoomState;

			if ( newBossRoomState == 1 ) // SummonFadeIn
				TryBeginBossStageSummonSequence();
			else if ( newBossRoomState == 3 ) // BossActive
				TryActivateBossStageBoss();
			else if ( newBossRoomState >= 4 ) // BossDead or Cleared
				MarkMegaGridClearedByNumber(5);
		}

        // Player 좌표 업데이트
		auto ComputePlayerMoveDir = [] (uint32_t moveDirBits, float yawDeg) -> XMFLOAT3
		{
			constexpr uint32_t kDirForward  = 0x01;
			constexpr uint32_t kDirBackward = 0x02;
			constexpr uint32_t kDirLeft     = 0x04;
			constexpr uint32_t kDirRight    = 0x08;

			const float yawRad = XMConvertToRadians(yawDeg);
			const XMVECTOR look = XMVectorSet(std::sinf(yawRad), 0.0f, std::cosf(yawRad), 0.0f);
			const XMVECTOR right = XMVectorSet(std::cosf(yawRad), 0.0f, -std::sinf(yawRad), 0.0f);
			XMVECTOR dir = XMVectorZero();

			if ( moveDirBits & kDirForward )  dir += look;
			if ( moveDirBits & kDirBackward ) dir -= look;
			if ( moveDirBits & kDirRight )    dir += right;
			if ( moveDirBits & kDirLeft )     dir -= right;

			if ( XMVectorGetX(XMVector3LengthSq(dir)) > 1e-8f )
				dir = XMVector3Normalize(dir);
			else
				dir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			XMFLOAT3 out{};
			XMStoreFloat3(&out, dir);
			return out;
		};

        for (const auto& state : snapshot.players)
        {
            // id를 slot으로 사용 (0~3)
            int slot = static_cast<int>(state.id);
            CGameObject* player = GetPlayerBySlot(slot);
            if (!player) continue;

			const bool isLocalPlayer = ( slot == m_localPlayerSlot );
			const XMFLOAT3 resolvedPosition =
				ResolveNetworkActorY(state.id, true, state.position, dt);

			if ( auto* hp = player->GetComponent<CHealthComponent>() )
				hp->SetCurrentHp(static_cast< int >( state.hp ));

			if ( isLocalPlayer )
			{
				constexpr float kLocalPlayerServerSnapDistance = 1.5f;
				constexpr float kLocalPlayerServerSnapDistanceSq =
					kLocalPlayerServerSnapDistance * kLocalPlayerServerSnapDistance;

                const XMFLOAT3 currentPos = player->GetPosition();
                const float dx = resolvedPosition.x - currentPos.x;
                const float dy = resolvedPosition.y - currentPos.y;
                const float dz = resolvedPosition.z - currentPos.z;
                const float distSq = dx * dx + dy * dy + dz * dz;

                if (distSq > kLocalPlayerServerSnapDistanceSq)
                {
                    player->SetPosition(resolvedPosition.x, resolvedPosition.y, resolvedPosition.z);

					if ( auto* tr = player->GetComponent<CTransformComponent>() )
						tr->SetYawDegrees(state.yaw);

					if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
						controller->SetYawDegrees(state.yaw);
                }
            }
            else
            {
				const DecodedAnimStateCode decodedDR =
					DecodeStateCode(state.animation.stateCode);
				float drSpeed = 0.0f;
				XMFLOAT3 moveDir = XMFLOAT3(0.0f, 0.0f, 1.0f);
				if ( decodedDR.hasMove && !decodedDR.die && !decodedDR.hit )
				{
					moveDir = ComputePlayerMoveDir(decodedDR.moveDirBits, state.yaw);
					if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
						drSpeed = decodedDR.run ? controller->GetRunMoveSpeed() : controller->GetWalkMoveSpeed();
					else
						drSpeed = decodedDR.run ? 10.0f : 5.0f;
				}

				PlayerDRState& dr = m_playerDRStates[state.id];
				if ( !dr.initialized )
				{
					dr.predictedPos = resolvedPosition;
					dr.initialized = true;
				}
				else
				{
					constexpr float kCorrectionAlpha = 0.35f;
					dr.predictedPos = LerpPosition(dr.predictedPos, resolvedPosition, kCorrectionAlpha);
				}
				dr.moveDir = moveDir;
				dr.speed = drSpeed;

                player->SetPosition(dr.predictedPos.x, dr.predictedPos.y, dr.predictedPos.z);

				// yaw 회전 적용
				if (auto* tr = player->GetComponent<CTransformComponent>())
				{
					tr->SetYawDegrees(state.yaw);
				}
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
					const int curAnimTick = state.animation.animTick;
					const auto tickIt = m_prevPlayerAnimTick.find(state.id);
					if (tickIt == m_prevPlayerAnimTick.end() || tickIt->second != curAnimTick)
					{
						m_prevPlayerAnimTick[state.id] = curAnimTick;
						if ( slot == m_localPlayerSlot )
							m_bLocalPlayerDead = true;
						ac->RequestDeath();
					}
                    ac->SetAnimState(EAnimState::Die);
					m_prevPlayerNetworkStateCode[state.id] = state.animation.stateCode;
					continue;
                }

				else if ( decoded.hit )
				{
					const int curAnimTick = state.animation.animTick;
					const auto tickIt = m_prevPlayerAnimTick.find(state.id);
					if (tickIt == m_prevPlayerAnimTick.end() || tickIt->second != curAnimTick)
					{
						m_prevPlayerAnimTick[state.id] = curAnimTick;
						SpawnBloodSplash(player, nullptr, nullptr);
						if ( auto* hp = player->GetComponent<CHealthComponent>() )
							hp->RequestHitSfx();
						ac->RequestHit();
					}
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
					const int curAnimTick = state.animation.animTick;
					const auto tickIt = m_prevPlayerAnimTick.find(state.id);
					if (tickIt == m_prevPlayerAnimTick.end() || tickIt->second != curAnimTick)
					{
						m_prevPlayerAnimTick[state.id] = curAnimTick;
						RequestPlayerAttackSfx(player);
						if ( state.weaponType == EWeaponType::Sword )
							BeginSwordTrail(player);
						else if ( state.weaponType == EWeaponType::Axe )
							BeginAxeTrail(player);
						else if ( state.weaponType == EWeaponType::Gun )
						{
							const XMFLOAT3 dirN = GetSafeObjectForward(player);
							XMFLOAT3 muzzlePos = player->GetPosition();
							muzzlePos.y += 1.15f;
							muzzlePos.x += dirN.x * 0.85f;
							muzzlePos.z += dirN.z * 0.85f;
							SpawnMuzzleFlash(muzzlePos, dirN);
						}
						ac->RequestAttack();
					}
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


		// Enemy 좌표 업데이트는 ClientContents의 logical monster 구조를 기준으로 한다.
		// TestMain의 보스 Call 소환 이펙트 기능만 logical state 위에 이식한다.
		auto MapBossCallSummonKind = [] (uint32_t enemyType, EEnemySpawnerEnemyKind& outKind) -> bool
			{
				switch ( enemyType )
				{
				case kNetworkEnemyTypeBasic:
					outKind = EEnemySpawnerEnemyKind::Ghoul;
					return true;
				case kNetworkEnemyTypeArcher:
					outKind = EEnemySpawnerEnemyKind::BowMan;
					return true;
				case kNetworkEnemyTypeWarrior:
					outKind = EEnemySpawnerEnemyKind::SwordMan;
					return true;
				case kNetworkEnemyTypeMutant:
					outKind = EEnemySpawnerEnemyKind::Mutant;
					return true;
				default:
					return false;
				}
			};

		auto MakeSpawnFxKey =
			[] (uint64_t enemyId, uint32_t spawnFxSerial) -> uint64_t
			{
				uint64_t key = enemyId;
				key ^= static_cast< uint64_t >( spawnFxSerial ) +
					0x9e3779b97f4a7c15ull +
					(key << 6) +
					(key >> 2);
				return key;
			};

		auto BossCallSummonCountForCall = [] (int callIndex) -> int
			{
				switch ( callIndex )
				{
				case 1: return 30;
				case 2: return 30;
				case 3: return 35;
				default: return 0;
				}
			};

		auto IsInsideNetworkBossCallSummonArea = [] (const XMFLOAT3& pos) -> bool
			{
				constexpr float kMargin = 5.0f;
				return pos.x >= -100.0f - kMargin &&
					pos.x <= 100.0f + kMargin &&
					pos.z >= 300.0f - kMargin &&
					pos.z <= 500.0f + kMargin;
			};

		for ( const EnemyState& state : snapshot.enemies )
		{
			if ( state.enemyType != kNetworkEnemyTypeBoss )
				continue;

			const DecodedAnimStateCode decoded = DecodeStateCode(state.animation.stateCode);
			const auto prevIt = m_prevEnemyNetworkStateCode.find(state.id);
			const uint32_t prevStateCode =
				( prevIt != m_prevEnemyNetworkStateCode.end() ) ? prevIt->second : 0u;
			const DecodedAnimStateCode prevDecoded = DecodeStateCode(prevStateCode);

			if ( decoded.bossCall && !prevDecoded.bossCall )
			{
				++m_networkBossCallIndex;
				m_networkBossCallPendingSummonEffects =
					BossCallSummonCountForCall(m_networkBossCallIndex);
				m_networkBossCallSummonEffectWindowSec =
					( m_networkBossCallPendingSummonEffects > 0 )
					? ( kBossCallMonsterSpawnDelaySec + 3.0f )
					: 0.0f;

				if ( m_networkBossCallPendingSummonEffects > 0 )
				{
					m_networkBossCallSummonEffectEnemyIds.clear();
					BeginNetworkBossCallMonsterSummonVisuals(
						m_networkBossCallIndex,
						kBossCallMonsterSpawnDelaySec
					);
				}
			}
		}

		if ( m_itemBillboardState.bossCallSummonCircleVisual.active )
		{
			for ( const EnemyState& state : snapshot.enemies )
			{
				if ( state.lifecycleState != static_cast< uint32_t >(Protocol::ENEMY_STATE_SPAWN_PENDING) )
					continue;

				constexpr uint32_t kSpawnFxBossCallSummon = 1;
				if ( state.spawnFxType != kSpawnFxBossCallSummon || state.spawnFxSerial == 0 )
					continue;

				const uint64_t previewKey = MakeSpawnFxKey(state.id, state.spawnFxSerial);
				if ( !m_networkBossCallSummonPreviewKeys.insert(previewKey).second )
					continue;

				EEnemySpawnerEnemyKind summonKind = EEnemySpawnerEnemyKind::Ghoul;
				if ( !MapBossCallSummonKind(state.enemyType, summonKind) )
					continue;

				BossCallSummonVisualPreview preview{};
				preview.kind = summonKind;
				preview.position = state.position;
				m_networkBossCallSummonVisualPreviews.push_back(preview);
				AddBossCallSummonCircle(preview.position, preview.kind);
			}
		}

		if ( m_networkBossCallSummonEffectWindowSec > 0.0f )
		{
			m_networkBossCallSummonEffectWindowSec -= dt;
			if ( m_networkBossCallSummonEffectWindowSec <= 0.0f )
			{
				m_networkBossCallSummonEffectWindowSec = 0.0f;
				m_networkBossCallPendingSummonEffects = 0;
				StartBossCallSummonCircleFadeOut();
			}
		}

		XMFLOAT3 bossCallSummonEffectPosSum(0.0f, 0.0f, 0.0f);
		int bossCallSummonEffectCountThisFrame = 0;
		const bool allowBossCallSpawnFxPlayback =
			GetLocalPlayerMegaGridNumberForMonsterTick() == 5;

		ApplyNetworkEnemySnapshotToLogicalMonsters(snapshot, dt);
		ReconcileLogicalMonsterVisualBindings();

		for ( const EnemyState& state : snapshot.enemies )
		{
			if ( state.lifecycleState == static_cast< uint32_t >(Protocol::ENEMY_STATE_SPAWN_PENDING) )
				continue;

			auto logicalIt = m_logicalMonsterIndexByServerId.find(state.id);
			if ( logicalIt == m_logicalMonsterIndexByServerId.end() )
			{
				m_prevEnemyNetworkStateCode[state.id] = state.animation.stateCode;
				continue;
			}

			const int logicalIndex = logicalIt->second;
			if ( logicalIndex < 0 || logicalIndex >= static_cast< int >(m_logicalMonsters.size()) )
			{
				m_prevEnemyNetworkStateCode[state.id] = state.animation.stateCode;
				continue;
			}

			LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];
			const XMFLOAT3 resolvedNetworkPosition =
				ResolveNetworkActorY(state.id, false, state.position, dt);

			constexpr uint32_t kSpawnFxBossCallSummon = 1;
			const bool hasBossCallSpawnFx =
				state.spawnFxType == kSpawnFxBossCallSummon &&
				state.spawnFxSerial != 0;
			const uint64_t spawnFxKey =
				MakeSpawnFxKey(state.id, state.spawnFxSerial);

			bool shouldPlayBossCallSpawnFx =
				allowBossCallSpawnFxPlayback &&
				hasBossCallSpawnFx &&
				m_playedSpawnFxKeys.insert(spawnFxKey).second;

			const auto prevNetworkPosIt = m_prevNetworkEnemyPositions.find(state.id);
			const bool wasInactiveByNetworkPosition =
				( prevNetworkPosIt != m_prevNetworkEnemyPositions.end() &&
				  prevNetworkPosIt->second.y <= kEnemySpawnerInactiveY + 1.0f ) ||
				( prevNetworkPosIt == m_prevNetworkEnemyPositions.end() &&
				  m_networkBossCallSummonEffectWindowSec > 0.0f );

			if ( !shouldPlayBossCallSpawnFx &&
				 allowBossCallSpawnFxPlayback &&
				 m_networkBossCallSummonEffectWindowSec > 0.0f &&
				 wasInactiveByNetworkPosition &&
				 IsInsideNetworkBossCallSummonArea(resolvedNetworkPosition) &&
				 m_networkBossCallSummonEffectEnemyIds.insert(state.id).second )
			{
				shouldPlayBossCallSpawnFx = true;
			}

			if ( shouldPlayBossCallSpawnFx )
			{
				EEnemySpawnerEnemyKind summonKind = EEnemySpawnerEnemyKind::Ghoul;

				if ( MapBossCallSummonKind(state.enemyType, summonKind) )
				{
					m_networkBossCallSummonEffectEnemyIds.insert(state.id);
					SpawnBossCallSummonWwwEffect(resolvedNetworkPosition, summonKind);

					bossCallSummonEffectPosSum.x += resolvedNetworkPosition.x;
					bossCallSummonEffectPosSum.y += resolvedNetworkPosition.y;
					bossCallSummonEffectPosSum.z += resolvedNetworkPosition.z;
					++bossCallSummonEffectCountThisFrame;

					if ( m_networkBossCallPendingSummonEffects > 0 )
					{
						--m_networkBossCallPendingSummonEffects;
						if ( m_networkBossCallPendingSummonEffects <= 0 )
						{
							m_networkBossCallPendingSummonEffects = 0;
							m_networkBossCallSummonEffectWindowSec = 0.0f;
							StartBossCallSummonCircleFadeOut();
						}
					}
				}
			}

			m_prevNetworkEnemyPositions[state.id] = resolvedNetworkPosition;

			if ( !logical.boundObject )
			{
				m_prevEnemyNetworkStateCode[state.id] = state.animation.stateCode;
				continue;
			}
			ApplyNetworkEnemyVisualSnapshot(logical.boundObject, logicalIndex, state, dt);
		}

		if ( bossCallSummonEffectCountThisFrame > 0 )
		{
			const float invCount = 1.0f / static_cast< float >( bossCallSummonEffectCountThisFrame );

			XMFLOAT3 sfxPos{};
			sfxPos.x = bossCallSummonEffectPosSum.x * invCount;
			sfxPos.y = bossCallSummonEffectPosSum.y * invCount;
			sfxPos.z = bossCallSummonEffectPosSum.z * invCount;

			PlayBossCallMonsterSpawnSfxAt(sfxPos);
		}

		// Projectile 동기화
		std::unordered_set<uint64_t> visibleArrowIds;
		std::unordered_set<uint64_t> visibleBulletIds;
		std::unordered_set<uint64_t> visibleBossPoisonIds;

		auto AcquireNetworkProjectile = [] (
			const std::vector<CGameObject*>& refs,
			const std::unordered_map<uint64_t, CGameObject*>& activeById) -> CGameObject*
		{
			for ( CGameObject* obj : refs )
			{
				if ( !obj ) continue;

				bool inUse = false;
				for ( const auto& pair : activeById )
				{
					if ( pair.second == obj )
					{
						inUse = true;
						break;
					}
				}

				if ( !inUse )
					return obj;
			}

			return nullptr;
		};

		auto ResolveProjectileDR = [ & ] (const BulletState& b) -> XMFLOAT3
		{
			ProjectileDRState& dr = m_projectileDRStates[b.id];
			if ( !dr.initialized )
			{
				dr.predictedPos = b.position;
				dr.initialized = true;
			}
			else
			{
				constexpr float kCorrectionAlpha = 0.35f;
				dr.predictedPos = LerpPosition(dr.predictedPos, b.position, kCorrectionAlpha);
			}

			dr.velocity = b.velocity;
			return dr.predictedPos;
		};

		for ( auto& [id, dr] : m_projectileDRStates )
		{
			if ( !dr.initialized )
				continue;

			dr.predictedPos.x += dr.velocity.x * dt;
			dr.predictedPos.y += dr.velocity.y * dt;
			dr.predictedPos.z += dr.velocity.z * dt;
		}

		for (const auto& b : snapshot.bullets)
		{
			const XMFLOAT3 predictedProjectilePos = ResolveProjectileDR(b);

			if (b.bulletType == 1u) // BULLET_TYPE_ARROW
			{
				visibleArrowIds.insert(b.id);

				CGameObject* arrowObj = nullptr;
				auto it = m_networkArrowById.find(b.id);
				if ( it != m_networkArrowById.end() )
					arrowObj = it->second;
				else
				{
					arrowObj = AcquireNetworkProjectile(m_arrowRefs, m_networkArrowById);
					if ( arrowObj )
						m_networkArrowById[b.id] = arrowObj;
				}

				if (!arrowObj) continue;

				if ( auto* arrow = arrowObj->GetComponent<CArrowComponent>() )
				{
					arrow->Activate(predictedProjectilePos, b.velocity, 2.0f, true);
					if ( auto* arrowtransform = arrowObj->GetComponent<CTransformComponent>() )
						arrowtransform->SetLookDirection(b.velocity);
				}
			}
			else if (b.bulletType == 3u) // BULLET_TYPE_BOSS_POISON
			{
				visibleBossPoisonIds.insert(b.id);

				int entryIdx = -1;
				auto it = m_networkBossPoisonById.find(b.id);
				if (it != m_networkBossPoisonById.end())
				{
					entryIdx = it->second;
				}
				else
				{
					auto& pool = m_bossPoisonProjectileEffect.entries;
					for (int i = 0; i < (int)pool.size(); ++i)
					{
						bool inUse = false;
						for (auto& kv : m_networkBossPoisonById)
							if (kv.second == i) { inUse = true; break; }
						if (!inUse) { entryIdx = i; m_networkBossPoisonById[b.id] = i; break; }
					}
				}

				if (entryIdx < 0 || entryIdx >= (int)m_bossPoisonProjectileEffect.entries.size()) continue;

				auto& entry = m_bossPoisonProjectileEffect.entries[entryIdx];
				entry.active   = true;
				entry.owner    = nullptr;
				entry.position = predictedProjectilePos;
				entry.velocity = b.velocity;

				float spd = std::sqrt(b.velocity.x*b.velocity.x + b.velocity.y*b.velocity.y + b.velocity.z*b.velocity.z);
				if (spd > 0.001f)
					entry.direction = XMFLOAT3(b.velocity.x/spd, b.velocity.y/spd, b.velocity.z/spd);

				entry.coreDiameter = 4.0f;
				entry.coreRadius   = 2.0f;
				entry.gasDiameter  = 8.0f;
				entry.speed        = 18.0f;
				entry.visualSeed   = static_cast<float>(b.id % 1000);
			}
			else // BULLET_TYPE_CANNONBALL etc.
			{
				visibleBulletIds.insert(b.id);

				CGameObject* bulletObj = nullptr;
				auto it = m_networkBulletById.find(b.id);
				if ( it != m_networkBulletById.end() )
					bulletObj = it->second;
				else
				{
					bulletObj = AcquireNetworkProjectile(m_bulletRefs, m_networkBulletById);
					if ( bulletObj )
						m_networkBulletById[b.id] = bulletObj;
				}

				if (!bulletObj) continue;

				if ( auto* bullet = bulletObj->GetComponent<CBulletComponent>() )
				{
					bullet->Activate(predictedProjectilePos, b.velocity, 2.0f, true);
				}
			}
		}

		for ( auto it = m_networkArrowById.begin(); it != m_networkArrowById.end(); )
		{
			if ( visibleArrowIds.find(it->first) != visibleArrowIds.end() )
			{
				++it;
				continue;
			}

			if ( it->second )
			{
				if ( auto* arrow = it->second->GetComponent<CArrowComponent>() )
					arrow->Deactivate();
			}
			m_projectileDRStates.erase(it->first);
			it = m_networkArrowById.erase(it);
		}

		for ( auto it = m_networkBulletById.begin(); it != m_networkBulletById.end(); )
		{
			if ( visibleBulletIds.find(it->first) != visibleBulletIds.end() )
			{
				++it;
				continue;
			}

			if ( it->second )
			{
				if ( auto* bullet = it->second->GetComponent<CBulletComponent>() )
					bullet->Deactivate();
			}
			m_projectileDRStates.erase(it->first);
			it = m_networkBulletById.erase(it);
		}

		for (auto it = m_networkBossPoisonById.begin(); it != m_networkBossPoisonById.end(); )
		{
			if (visibleBossPoisonIds.count(it->first)) { ++it; continue; }

			int idx = it->second;
			if (idx >= 0 && idx < (int)m_bossPoisonProjectileEffect.entries.size())
				m_bossPoisonProjectileEffect.entries[idx].active = false;
			m_projectileDRStates.erase(it->first);
			it = m_networkBossPoisonById.erase(it);
		}

		// 서버 item snapshot으로 billboard 상태 reconcile
		for ( const ItemSpawnState& itemState : receivedSnapshot.items )
		{
			for ( ItemBillboardEntry& entry : m_itemBillboardState.entries )
			{
				if ( entry.serverId == itemState.id )
				{
					const bool wasActive = entry.active;
					entry.active = itemState.active;
					if ( !itemState.active )
					{
						entry.distanceCulled = true;
						if ( wasActive && entry.kind == EItemBillboardKind::Key )
							MarkMegaGridClearedByNumber(entry.megaGridNumber);
					}
					break;
				}
			}
		}

		// 로컬 플레이어 인벤토리 동기화
		for ( const auto& playerState : receivedSnapshot.players )
		{
			if ( static_cast<int>(playerState.id) != m_localPlayerSlot )
				continue;

			std::array<int, CGameSceneHUD::kInventorySlotCount> counts = { 0, 0, 0, 0 };
			for ( const InventoryEntryState& inv : playerState.inventory )
			{
				const int slot = static_cast<int>(inv.kind) - 1;
				if ( slot >= 0 && slot < CGameSceneHUD::kInventorySlotCount )
					counts[static_cast<size_t>(slot)] = inv.count;
			}
			SetInventoryItemCounts(counts);
			break;
		}

		// 사용이 끝난 data는 기본값으로 초기화 (선택적)
		m_pendingNetworkMessage.data = LoadoutData{};
    }
	else
	{
		m_timeSinceLastFramePacket += dt;

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

			if ( static_cast< int >( id ) == m_localPlayerSlot )
				continue;

			auto drIt = m_playerDRStates.find(id);
			if ( drIt == m_playerDRStates.end() ) continue;

			PlayerDRState& dr = drIt->second;
			if ( !dr.initialized || dr.speed <= 0.0f ) continue;

			dr.predictedPos.x += dr.moveDir.x * dr.speed * dt;
			dr.predictedPos.z += dr.moveDir.z * dr.speed * dt;

			player->SetPosition(dr.predictedPos.x, dr.predictedPos.y, dr.predictedPos.z);
		}

		for ( const auto& [id, stateCode] : m_prevEnemyNetworkStateCode )
		{
			auto logicalIt = m_logicalMonsterIndexByServerId.find(id);
			if ( logicalIt == m_logicalMonsterIndexByServerId.end() )
				continue;

			const int logicalIndex = logicalIt->second;
			if ( logicalIndex < 0 || logicalIndex >= static_cast< int >(m_logicalMonsters.size()) )
				continue;

			LogicalMonsterState& logical = m_logicalMonsters[static_cast< size_t >(logicalIndex)];
			CGameObject* obj = logical.boundObject;

			const DecodedAnimStateCode decoded = DecodeStateCode(stateCode);

			if ( obj )
			{
				if ( auto* animComp = obj->GetComponent<CAnimatorComponent>() )
				{
					if ( auto* ctrl = animComp->EnsureMonsterController() )
					{
						EMonsterAnimState locomotionState = EMonsterAnimState::Idle;

						if ( decoded.hasMove )
							locomotionState = decoded.run ? EMonsterAnimState::Run : EMonsterAnimState::Move;

						ctrl->SetLocomotionState(locomotionState);
					}
				}
			}

			auto drIt = m_enemyDRStates.find(id);
			if ( drIt == m_enemyDRStates.end() )
				continue;

			EnemyDRState& dr = drIt->second;
			if ( !dr.initialized || dr.speed <= 0.0f )
				continue;

			dr.predictedPos.x += dr.moveDir.x * dr.speed * dt;
			dr.predictedPos.z += dr.moveDir.z * dr.speed * dt;

			const int oldMegaGridNumber = logical.megaGridNumber;

			logical.position = dr.predictedPos;
			logical.megaGridNumber = m_sceneGrid.MegaGridNumberFromWorldPosition(logical.position.x, logical.position.z);

			UpdateLogicalMonsterMegaGridIndex(logicalIndex, oldMegaGridNumber);

			if ( obj )
			{
				obj->SetPosition(logical.position.x, logical.position.y, logical.position.z);

				if ( auto* tr = obj->GetComponent<CTransformComponent>() )
					tr->SetYawDegrees(logical.yawDeg);
			}
		}

		for ( auto& [id, dr] : m_projectileDRStates )
		{
			if ( !dr.initialized )
				continue;

			dr.predictedPos.x += dr.velocity.x * dt;
			dr.predictedPos.y += dr.velocity.y * dt;
			dr.predictedPos.z += dr.velocity.z * dt;
		}

		for ( const auto& [id, entryIndex] : m_networkBossPoisonById )
		{
			if ( entryIndex < 0 || entryIndex >= static_cast< int >(m_bossPoisonProjectileEffect.entries.size()) )
				continue;

			auto drIt = m_projectileDRStates.find(id);
			if ( drIt == m_projectileDRStates.end() )
				continue;

			ProjectileDRState& dr = drIt->second;
			if ( !dr.initialized )
				continue;

			BossPoisonProjectileEntry& entry = m_bossPoisonProjectileEffect.entries[static_cast< size_t >(entryIndex)];
			entry.position = dr.predictedPos;
			entry.velocity = dr.velocity;
		}

		RebuildSceneGridMonsterRefsFromLogicalBindings();

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
		if ( IsBossMonsterObject(obj) && !obj->GetActive() )
		{
			if ( cache.animator )
				cache.animator->SetPoseEvaluationEnabled(false);

			continue;
		}
#endif

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
		{
#ifndef USING_NETWORK
			SyncLogicalMonsterFromActualObject(obj);
#endif
			continue;
		}

		obj->Animate(dt);

#ifndef USING_NETWORK
		SyncLogicalMonsterFromActualObject(obj);
#endif
	}

#ifndef USING_NETWORK
	UpdateInventoryBuffAmbientParticles(dt);

	if ( CInventoryComponent* inventory = GetLocalPlayerInventory() )
	{
		if ( inventory->ConsumeAttackPowerDirty() )
			RefreshPlayerWeaponAttackPowersForSlot(m_localPlayerSlot);
	}

	SyncLocalInventoryToHud();
#endif

	UpdateBossMeleeSlashCasts(dt);

#ifndef USING_NETWORK
	UpdateBossPoisonProjectileSpellCasts(dt);
	UpdateBossPoisonProjectiles(dt);
#endif

	UpdateDynamicGridState();

	UpdateBossStageBgmState();

	UpdateMegaGrid4LowYPoison(dt);

	UpdatePlayerFootstepSfx();
	UpdateMonsterSfx(dt);
	UpdateMonsterHpGaugeTimers(dt);

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

	UpdateArrowTrails(dt);

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

bool CGameScene::IsBossStageBossAppearFinishedForHud(CGameObject* boss) const
{
	if ( !boss )
		return false;

	const auto it = m_bossStageBossPositionStates.find(boss);

	if ( it == m_bossStageBossPositionStates.end() )
		return false;

	return it->second.appearFinished;
}

bool CGameScene::ShouldRenderBossHpGaugeHud(CGameObject* boss) const
{
	if ( !boss )
		return false;

	if ( !boss->GetActive() )
		return false;

	if ( !IsBossStageBossAppearFinishedForHud(boss) )
		return false;

	CGameObject* localPlayer = GetPlayer();

	if ( !localPlayer )
		localPlayer = GetPlayerBySlot(0);

	if ( !IsPlayerInsideBossStageBattleArea(localPlayer) )
		return false;

	const CHealthComponent* health = boss->GetComponent<CHealthComponent>();

	if ( !health )
		return false;

	if ( health->GetCurrentHp() <= 0 || health->GetMaxHp() <= 0 )
		return false;

	return true;
}

void CGameScene::UpdateBossHpGaugeHud()
{
#ifndef USING_NETWORK
	UpdateBossStageBossRenderGate();
#endif

	CGameObject* boss = FindBossStageBossInMegaGrid(5);

	if ( !ShouldRenderBossHpGaugeHud(boss) )
	{
		m_hud.SetBossHealthRatio(1.0f, false);
		return;
	}

	const CHealthComponent* health = boss->GetComponent<CHealthComponent>();

	if ( !health )
	{
		m_hud.SetBossHealthRatio(1.0f, false);
		return;
	}

	m_hud.SetBossHealthRatio(health->GetHpRatio(), true);
}

void CGameScene::UpdateShaderVariables(ID3D12GraphicsCommandList* /*cmd*/)
{
	PROFILE_RENDER_SCOPE("GameScene::UpdateShaderVariables");
	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	if ( m_pAudioManager )
	{
		if ( CMusicDirector* music = m_pAudioManager->GetMusicDirector() )
		{
			CGameObject* localPlayer = GetPlayer();
			if ( !localPlayer )
				localPlayer = GetPlayerBySlot(0);

			const float seaBlend = localPlayer ? ComputeSeaBgmBlendForPosition(localPlayer->GetPosition()) : 0.0f;
			music->SetGameplaySeaBlend(seaBlend);
		}
	}

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

	TERRAIN* mappedTerrain = m_pcbMappedTerrain[frameIndex];

	if ( mappedTerrain )
	{
		const XMFLOAT3 terrainScale = m_TerrainData->GetScale();
		const UINT heightMapSrvIndex = m_TerrainData->GetsrvIndex();

		mappedTerrain->gvTerrainScale = XMFLOAT4(
			terrainScale.x,
			terrainScale.y,
			terrainScale.z,
			(heightMapSrvIndex == UINT_MAX) ? 0.0f : 1.0f
		);

		mappedTerrain->gvTerrainHeightMapSize = XMFLOAT4(
			static_cast<float>(m_TerrainData->GetHeightMapWidth()),
			static_cast<float>(m_TerrainData->GetHeightMapLength()),
			0.0f,
			0.0f
		);

		mappedTerrain->gvTerrainTextureIndices = XMUINT4(
			heightMapSrvIndex,
			UINT_MAX,
			UINT_MAX,
			UINT_MAX
		);

		mappedTerrain->gvTerrainDiffuseTextureIndices = XMUINT4(
			m_TerrainData->GetGrassDiffuseSrvIndex(),
			m_TerrainData->GetGroundDiffuseSrvIndex(),
			m_TerrainData->GetDirtDiffuseSrvIndex(),
			UINT_MAX
		);

		mappedTerrain->gvTerrainNormalTextureIndices = XMUINT4(
			m_TerrainData->GetGrassNormalSrvIndex(),
			m_TerrainData->GetGroundNormalSrvIndex(),
			m_TerrainData->GetDirtNormalSrvIndex(),
			UINT_MAX
		);

		mappedTerrain->gvTerrainBlendParams = XMFLOAT4(
			400.0f,
			200.0f,
			1.0f,
			0.08f
		);
	}

	WATER* mappedWater = m_pcbMappedWater[frameIndex];

	if ( mappedWater )
	{
		mappedWater->gvWaterParams = XMFLOAT4(
			m_waterAccumulatedTime,
			m_waterHeight,
			m_waterBaseUvScale,
			m_waterAlpha
		);

		mappedWater->gvWaterTextureIndices = XMUINT4(
			m_waterBaseSrvIndex,
			m_waterDetail0SrvIndex,
			m_waterDetail1SrvIndex,
			UINT_MAX
		);

		mappedWater->gvWaterFlowParams = XMFLOAT4(
			0.0f, 0.00125f,   // base flow
			0.02f, 0.01f      // detail0 flow
		);

		mappedWater->gvWaterDetailParams = XMFLOAT4(
			-0.01f, 0.015f,   // detail1 flow
			10.0f,            // detail0 uv scale
			5.0f              // detail1 uv scale
		);

		XMStoreFloat4x4(
			&mappedWater->gf4x4TextureAnimation,
			XMMatrixIdentity()
		);
	}

	m_shadowMap.UpdateData(shadowFocus, directionalLightObj);
	m_shadowMap.UploadConstantBuffer();

	UpdateDepthFogState(m_fElapsedTime);
	m_depthFog.UploadConstantBuffer();
	UpdateSsaoCB(m_pMainCamera);

	{
		float localHpRatio = 1.0f;

		std::array<float, 4> playerHpRatios = { 0.0f, 0.0f, 0.0f, 0.0f };
		std::array<bool, 4> playerHpVisible = { false, false, false, false };

		for ( int slot = 0; slot < 4; ++slot )
		{
			CGameObject* player = GetPlayerBySlot(slot);

			if ( !player )
				continue;

			CHealthComponent* hp = player->GetComponent<CHealthComponent>();

			if ( !hp )
				continue;

			playerHpRatios[slot] = hp->GetHpRatio();
			playerHpVisible[slot] = true;
		}

		if ( m_localPlayerSlot >= 0 && m_localPlayerSlot < 4 && playerHpVisible[m_localPlayerSlot] )
		{
			localHpRatio = playerHpRatios[m_localPlayerSlot];
		}
		else
		{
			CGameObject* localPlayer = GetPlayer();

			if ( !localPlayer )
				localPlayer = GetPlayerBySlot(0);

			if ( localPlayer )
			{
				if ( auto* hp = localPlayer->GetComponent<CHealthComponent>() )
					localHpRatio = hp->GetHpRatio();
			}
		}

		const std::array<bool, 4> playerWorldHpGaugeVisible = m_otherPlayerWorldHpGaugeVisibleForHud;

		m_hud.SetHealthRatio(localHpRatio);
		m_hud.SetOtherPlayerHealthRatios(m_localPlayerSlot, playerHpRatios, playerHpVisible, playerWorldHpGaugeVisible);
		UpdateBossHpGaugeHud();
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

	ApplyAttachmentCullFromSkinnedOwners(camera);

	{
		PROFILE_RENDER_SCOPE("UFRS::BuildStaticVisibleListsForFrame");
		BuildStaticVisibleListsForFrame(camera);
	}

	UpdateOtherPlayerWorldHpGaugeVisibilityForHud(camera);
}

bool CGameScene::IsOtherPlayerSkinnedBodyRenderedThisFrame(int playerSlot, CCamera* camera, UINT& outSkinnedBatchObjectIndex) const
{
	outSkinnedBatchObjectIndex = UINT_MAX;

	if ( playerSlot < 0 || playerSlot >= 4 )
		return false;

	if ( playerSlot == m_localPlayerSlot )
		return false;

	CGameObject* player = GetPlayerBySlot(playerSlot);

	if ( !player )
		return false;

	if ( !player->GetActive() )
		return false;

	if ( !FindSkinnedBatchObjectIndex(player, outSkinnedBatchObjectIndex) )
		return false;

	if ( outSkinnedBatchObjectIndex >= static_cast< UINT >( m_skinnedBatch.objectRefs.size() ) )
		return false;

	if ( outSkinnedBatchObjectIndex < static_cast< UINT >(m_skinnedDistanceCullFlags.size()) && m_skinnedDistanceCullFlags[outSkinnedBatchObjectIndex] != 0 )
		return false;

	if ( outSkinnedBatchObjectIndex < static_cast< UINT >(m_skinnedOcclusionCullFlags.size()) && m_skinnedOcclusionCullFlags[outSkinnedBatchObjectIndex] != 0 )
		return false;

	if ( camera && !player->IsVisible(camera) )
		return false;

	const CSkinnedMeshRendererComponent* renderer = player->GetComponent<CSkinnedMeshRendererComponent>();

	if ( !renderer || !renderer->IsEnabled() )
		return false;

	const CSkinningComponent* skin = player->GetComponent<CSkinningComponent>();

	if ( !skin || !skin->IsSkinned() )
		return false;

	const CHealthComponent* health = player->GetComponent<CHealthComponent>();

	if ( !health )
		return false;

	if ( health->GetCurrentHp() <= 0 || health->GetMaxHp() <= 0 )
		return false;

	return true;
}

void CGameScene::UpdateOtherPlayerWorldHpGaugeVisibilityForHud(CCamera* camera)
{
	m_otherPlayerWorldHpGaugeVisibleForHud.fill(false);

	for ( int slot = 0; slot < 4; ++slot )
	{
		UINT skinnedBatchObjectIndex = UINT_MAX;

		if ( IsOtherPlayerSkinnedBodyRenderedThisFrame(slot, camera, skinnedBatchObjectIndex) )
			m_otherPlayerWorldHpGaugeVisibleForHud[slot] = true;
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

	if ( m_pd3dcbTerrain[frameIndex] )
	{
		cmd->SetGraphicsRootConstantBufferView(
			ROOT_PARAMETER_TERRAIN,
			m_pd3dcbTerrain[frameIndex]->GetGPUVirtualAddress()
		);
	}

	if ( m_pd3dcbWater[frameIndex] )
	{
		cmd->SetGraphicsRootConstantBufferView(
			ROOT_PARAMETER_WATER,
			m_pd3dcbWater[frameIndex]->GetGPUVirtualAddress()
		);
	}

	if ( m_pd3dcbSsao[frameIndex] )
	{
		cmd->SetGraphicsRootConstantBufferView(
			ROOT_PARAMETER_SSAO,
			m_pd3dcbSsao[frameIndex]->GetGPUVirtualAddress()
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

void CGameScene::RenderSkyBox(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd || !camera )
		return;

	if ( !m_skyBox.shader || !m_skyBox.vertexBuffer || m_skyBox.vertexCount == 0 )
		return;

	if ( m_skyBox.textureBaseSrvIndex == UINT_MAX )
		return;

	CScene::OnPrepareRender(cmd, camera);
	BindFrameRootParameters(cmd);

	m_skyBox.shader->Render(cmd, camera, nullptr);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &m_skyBox.vertexBufferView);
	cmd->SetGraphicsRoot32BitConstant(
		ROOT_PARAMETER_MATERIAL_ID,
		m_skyBox.textureBaseSrvIndex,
		0
	);
	cmd->DrawInstanced(m_skyBox.vertexCount, 1, 0, 0);
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

	if ( m_itemBillboardState.shader )
	{
		RenderItemBillboards(cmd, camera);
	}

	if ( m_skinnedBatch.shader )
	{
		RenderSkinnedInstanceGroups(cmd, camera);
	}

	if ( m_monsterHpGaugeState.shader )
	{
		RenderMonsterHpGauges(cmd, camera);
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
						music->RequestState(EMusicState::None, false);
						music->BeginPendingTransition();
					}
					else
					{
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

	RenderSkyBox(cmd, camera);

	if ( m_transparentWaterShader )
	{
		RenderTransparentWaterInstanceGroups(cmd, camera);
	}

	if ( m_itemBillboardState.transparentShader )
	{
		RenderTransparentItemBillboards(cmd, camera);
	}

	if ( m_bossCallSummonWwwEffect.shader )
	{
		RenderBossCallSummonWwwEffects(cmd, camera);
	}

	if ( m_bossPoisonProjectileEffect.shader )
	{
		RenderBossPoisonProjectiles(cmd, camera);
	}

	if ( m_swordTrailEffect.shader )
	{
		RenderSwordTrails(cmd, camera);
	}

	if ( m_monsterSwordTrailEffect.shader )
	{
		RenderMonsterSwordTrails(cmd, camera);
	}

	if ( m_arrowTrailEffect.shader )
	{
		RenderArrowTrails(cmd, camera);
	}

	if ( m_monsterArrowTrailEffect.shader )
	{
		RenderMonsterArrowTrails(cmd, camera);
	}

	if ( m_gunSmokeEffect.shader )
	{
		RenderGunSmokes(cmd, camera);
	}

	if ( m_muzzleFlashEffect.shader )
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
