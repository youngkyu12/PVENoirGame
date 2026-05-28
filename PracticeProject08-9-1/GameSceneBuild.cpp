//-----------------------------------------------------------------------------
// File: GameSceneBuild.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"
#include "GameSceneHelper.h"

using namespace GameSceneHelper;
namespace
{
	template <typename T>
	void ClearVectorAndFreeMemory(std::vector<T>& v)
	{
		std::vector<T>().swap(v);
	}

	template <typename T>
	void ClearUnorderedSetAndFreeMemory(std::unordered_set<T>& s)
	{
		std::unordered_set<T>().swap(s);
	}
}

void CGameScene::ConfigureLocalGameplaySimulationSwitches()
{
#ifdef USING_NETWORK
	m_bSimulateLocalPlayerMonsterAttackCollision = false;

	m_bSimulateLocalAI = false;

	m_bSimulateLocalGhoulAI = false;
	m_bSimulateLocalBowManAI = false;
	m_bSimulateLocalSwordManAI = false;
	m_bSimulateLocalMutantAI = false;
	m_bSimulateLocalBossAI = false;
	m_bSimulateLocalBossSummon = false;
	m_bSimulateLocalBossStageMonsterAI = false;

	m_bSimulateLocalMonsterChase = false;
	m_bSimulateLocalEnemySpawner = true;
	m_bSimulateLocalPlayerWorldStaticRollback = true;
	m_bSimulateLocalTeleport = false;
	m_bSimulateLocalItemPickup = true;
	m_bCanBossStageDirectly = false;
	m_bSimulateLocalStageTeleport = false;
#else
	m_bSimulateLocalPlayerMonsterAttackCollision = true;

	m_bSimulateLocalAI = true;

	m_bSimulateLocalGhoulAI = false;
	m_bSimulateLocalBowManAI = true;
	m_bSimulateLocalSwordManAI = false;
	m_bSimulateLocalMutantAI = false;
	m_bSimulateLocalBossAI = false;
	m_bSimulateLocalBossSummon = false;
	m_bSimulateLocalBossStageMonsterAI = false;

	m_bSimulateLocalMonsterChase = true;
	m_bSimulateLocalEnemySpawner = true;
	m_bSimulateLocalPlayerWorldStaticRollback = true;
	m_bSimulateLocalTeleport = true;
	m_bSimulateLocalItemPickup = true;
	m_bCanBossStageDirectly = true;
	m_bSimulateLocalStageTeleport = true;
#endif

	if ( !m_bSimulateLocalAI )
	{
		m_bSimulateLocalGhoulAI = false;
		m_bSimulateLocalBowManAI = false;
		m_bSimulateLocalSwordManAI = false;
		m_bSimulateLocalMutantAI = false;
		m_bSimulateLocalBossAI = false;
		m_bSimulateLocalBossSummon = false;
		m_bSimulateLocalBossStageMonsterAI = false;
	}

	if ( m_bSimulateLocalBossAI )
	{
		m_bSimulateLocalBossSummon = true;
	}

	m_bPrevLocalMonsterChaseToggleKeyDown = false;
	m_bPrevDebugDamageMegaGrid5KeyDown = false;

	m_bBossStageBossActivated = false;
	m_bBossSummonSequenceStarted = false;
	m_bBossSummonCircleFadeAgeSec = 0.0f;
	m_pendingBossStageBoss = nullptr;

	m_bPrevLocalStageTeleportKeyDown.fill(false);
}

void CGameScene::BuildObjects(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
	ConfigureLocalGameplaySimulationSwitches();

	ResetPlayerFootstepSfxState();
	ResetMonsterSfxState();

	m_playerWeaponOwnerByObject.clear();
	m_deadMonsters.clear();

	ResetEnemySpawnerTimedGhoulWaveStates();

	m_bBossStageBossActivated = false;
	m_bBossSummonSequenceStarted = false;
	m_bBossSummonCircleFadeAgeSec = 0.0f;
	m_pendingBossStageBoss = nullptr;

	m_bossStageBossPositionStates.clear();

	m_bLocalPlayerDead = false;
	m_bLocalPlayerRespawnUsed = false;
	m_localPlayerRespawnTimer = 0.0f;
	ResetBossPoisonProjectileState();

#ifdef USING_NETWORK
	m_prevPlayerNetworkStateCode.clear();
	m_prevEnemyNetworkStateCode.clear();

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

	GameSceneStageFileSet networkStageFiles{};
	if ( !ResolveStageFileSetFromMapId(gameStartData.mapId, networkStageFiles) )
	{
		assert(false && "Unknown mapId received from server");
		return;
	}

	if ( !LoadStaticPlacementFile(networkStageFiles.placementFilePath) )
	{
		assert(false && "Failed to load placement data for mapId");
		return;
	}

	if ( !LoadSceneCubeBoxColliderReport(networkStageFiles.cubeColliderReportFilePath) )
	{
		assert(false && "Failed to load cube box collider report for mapId");
		return;
	}

	ApplyStaticPlacementCounts();

	m_ghoulCount = 0;
	m_swordManCount = 0;
	m_bowManCount = 0;
	m_MutantCount = 0;
	m_bossCount = 0;

	for ( const EnemyState& enemyState : gameStartData.enemies )
	{
		switch ( enemyState.enemyType )
		{
		case kNetworkEnemyTypeArcher:
			++m_bowManCount;
			break;
		case kNetworkEnemyTypeWarrior:
			++m_swordManCount;
			break;
		case kNetworkEnemyTypeBoss:
			++m_bossCount;
			break;
		case kNetworkEnemyTypeMutant:
			++m_MutantCount;
			break;
		case kNetworkEnemyTypeNone:
		case kNetworkEnemyTypeBasic:
		default:
			++m_ghoulCount;
			break;
		}
	}
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

#ifndef USING_NETWORK
	m_EnemySpawnGhoulCount =
		kEnemySpawnerMega6GhoulCount +
		kEnemySpawnerMega8GhoulCount +
		kEnemySpawnerMega5GhoulCount;

	m_EnemySpawnBowManCount = kEnemySpawnerMega5BowManCount;
	m_EnemySpawnSwordManCount = kEnemySpawnerMega5SwordManCount;
	m_EnemySpawnMutantCount = kEnemySpawnerMega5MutantCount;

	m_EnemySpawnCount =
		m_EnemySpawnGhoulCount +
		m_EnemySpawnBowManCount +
		m_EnemySpawnSwordManCount +
		m_EnemySpawnMutantCount;

	// 스포너 몬스터도 실제 CGameObject 풀로 미리 만들어야 하므로
	// 타입별 총량에 더한다.
	// 단, 실제 기본 배치 루프는 spawn file 개수만 돌고,
	// 스포너 풀은 별도 루프에서 생성한다.
	m_ghoulCount += m_EnemySpawnGhoulCount;
	m_bowManCount += m_EnemySpawnBowManCount;
	m_swordManCount += m_EnemySpawnSwordManCount;
	m_MutantCount += m_EnemySpawnMutantCount;
#else
	m_EnemySpawnGhoulCount = 0;
	m_EnemySpawnBowManCount = 0;
	m_EnemySpawnSwordManCount = 0;
	m_EnemySpawnMutantCount = 0;
	m_EnemySpawnCount = 0;
#endif

	m_helmetCount = m_MutantCount;
	m_terrainCount = 1;

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
		m_swordManCount +
		m_terrainCount;

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
	InitializeSpatialGrid();

	auto pStaticShader = std::make_shared<CStaticObjectsShader>();
	auto pTreeStaticShader = std::make_shared<CTreeStaticObjectsShader>();
	auto pSkinnedShader = std::make_shared<CSkinnedObjectsShader>();
	auto pSkinnedAlphaClipShader = std::make_shared<CAlphaClipSkinnedObjectsShader>();
	auto pColliderShader = std::make_shared<CDiffusedShader>();
	auto pOcclusionStaticShader = std::make_shared<COcclusionStaticShader>();
	auto pShadowStaticShader = std::make_shared<CShadowMapStaticShader>();
	auto pShadowAlphaClipStaticShader = std::make_shared<CShadowMapAlphaClipStaticShader>();
	auto pShadowSkinnedShader = std::make_shared<CShadowMapSkinnedShader>();
	auto pShadowAlphaClipSkinnedShader = std::make_shared<CShadowMapAlphaClipSkinnedShader>();

	m_staticBatch.shader = pStaticShader;
	m_treeStaticShader = pTreeStaticShader;
	m_skinnedBatch.shader = pSkinnedShader;
	m_skinnedAlphaClipShader = pSkinnedAlphaClipShader;
	m_colliderBatch.shader = pColliderShader;
	m_occlusionStaticShader = pOcclusionStaticShader;
	m_shadowStaticShader = pShadowStaticShader;
	m_shadowAlphaClipStaticShader = pShadowAlphaClipStaticShader;
	m_shadowSkinnedShader = pShadowSkinnedShader;
	m_shadowAlphaClipSkinnedShader = pShadowAlphaClipSkinnedShader;

	DXGI_FORMAT rtvFormats[5] =
	{
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R32_FLOAT
	};

	BuildLightsAndMaterials();
	m_hud.BuildResources(dev, cmd, GetGraphicsRootSignature());
	m_hud.SetInactiveOverlayVisible(m_bInactiveOverlayVisible);
	BuildDepthFogResources(dev, cmd);
	m_shadowMap.BuildResources(dev, cmd, m_pDescriptorHeap.get());

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

	pTreeStaticShader->CreateShader(dev, m_pd3dGraphicsRootSignature.Get(), kRTCount, rtvFormats, kDsvFormat);
	pOcclusionStaticShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		0,
		nullptr,
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);

	pShadowStaticShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		0,
		nullptr,
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);

	pShadowAlphaClipStaticShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		0,
		nullptr,
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);

	pShadowSkinnedShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		0,
		nullptr,
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);

	pShadowAlphaClipSkinnedShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		0,
		nullptr,
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);

	pSkinnedAlphaClipShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		kRTCount,
		rtvFormats,
		kDsvFormat
	);

	BuildStaticBatch(dev, cmd, pStaticShader, kRTCount, rtvFormats, kDsvFormat);
	BuildItemBillboardBatch(dev, cmd, kRTCount, rtvFormats, kDsvFormat);

#ifndef USING_NETWORK
	//DumpStaticGridOccupancyLog();
	//BuildStaticWorldSubmeshOOBBDebugObjects(dev, cmd);
#endif
	BuildSkinnedBatch(dev, cmd, pSkinnedShader, kRTCount, rtvFormats, kDsvFormat);

#ifndef USING_NETWORK
	if ( !m_enemySpawner )
		m_enemySpawner = std::make_unique<EnemySpawner>();

	m_enemySpawner->Initialize(m_enemySpawnPoolEntries);
#endif

	for ( const SkinnedComponentCache& cache : m_skinnedComponentCache )
	{
		if ( cache.health )
			cache.health->SetAudioManager(m_pAudioManager);
	}

	LinkSceneObjects();

	CreateShaderVariables(dev, cmd);

	CGameObject* local = GetPlayer();
	if ( !local )
		local = GetPlayerBySlot(0);

	CreateMainCamera(dev, cmd, local);
	BuildObjectsCollider();

	RebuildDynamicGridState();

	ReleaseBuildOnlySceneData();

#ifdef USING_NETWORK
	Protocol::C_CLIENT_READY iamReady;

	iamReady.set_ready(true);
	iamReady.set_playerid(g_myPlayerId);
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(iamReady);
	g_clientService->BroadCast(sendBuffer);
#endif
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
	b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		b->cbGameObjects[frameIndex] = ::CreateBufferResource(
			dev, cmd, nullptr,
			b->cbElementBytes * cap,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			nullptr
		);

		if ( b->cbGameObjects[frameIndex] )
		{
			b->cbGameObjects[frameIndex]->Map(
				0,
				nullptr,
				reinterpret_cast< void** >( &b->mappedGameObjects[frameIndex] )
			);
		}

		b->baseCbvGpu[frameIndex] = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();

		m_pDescriptorHeap->CreateConstantBufferViews(
			dev,
			cap,
			b->cbGameObjects[frameIndex].Get(),
			b->cbElementBytes
		);
	}

	m_treeAlphaClipObjects.clear();
	m_treeAlphaClipObjects.reserve(cap);

	m_staticObjects.clear();
	m_staticObjects.reserve(cap);

	b->objectRefs.clear();
	b->objectRefs.reserve(cap);

#ifndef USING_NETWORK
	m_towerDoorPortals.clear();
	m_towerDoorPortals.reserve(m_towerCount);

	m_castleDoorPortals.clear();
	m_castleDoorPortals.reserve(m_castleCount);
#endif

	m_staticShadowCasterFlags.clear();
	m_staticShadowCasterFlags.reserve(cap);

	m_staticTreeObjectIndices.clear();
	m_staticTreeObjectIndices.reserve(cap);

	m_staticShadowOcclusionEntryIndices.clear();
	m_staticShadowOcclusionEntryIndices.reserve(cap);

	m_staticCollisionMegaGridMasks.clear();
	m_staticCollisionMegaGridMasks.reserve(cap);

	m_staticDynamicWorldMatrixFlags.clear();
	m_staticDynamicWorldMatrixFlags.reserve(cap);

	b->count = 0;

	ResetStaticWorldLodEntries();
	m_staticWorldLodEntries.reserve(m_staticPlacementEntries.size());

	std::vector<size_t> exportedWorldStaticPlacementIndices;
	std::vector<CGameObject*> exportedWorldStaticObjects;

	exportedWorldStaticPlacementIndices.reserve(m_staticPlacementEntries.size());
	exportedWorldStaticObjects.reserve(m_staticPlacementEntries.size());

	auto MakeStaticContext = [ & ] (UINT objectIndex)
		{
			const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

			GameSceneObjectFactory::CreateContext ctx{};
			ctx.device = dev;
			ctx.cmd = cmd;
			ctx.mappedGameObjectCB =
				reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >( b->mappedGameObjects[frameIndex] ) +
					objectIndex * b->cbElementBytes
				);
			ctx.cbvGpuHandle.ptr =
				b->baseCbvGpu[frameIndex].ptr +
				static_cast< UINT64 >( objectIndex ) * b->cbvInc;
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

		const bool logCastleVillageWallColliderBuild =
			kEnableCastleVillageWallColliderBuildLog &&
			( placement.assetName == "Castle" || placement.assetName == "VillageWall" );

		createDesc.debugColliderBuildLog = logCastleVillageWallColliderBuild;
		createDesc.debugColliderAssetName = placement.assetName;
		createDesc.debugColliderObjectName = placement.objectName;

		if ( createWorldStaticCollider )
		{
			const auto authoredIt = mSceneCubeBoxColliderTable.find(placement.assetName);
			if ( authoredIt != mSceneCubeBoxColliderTable.end() )
			{
				createDesc.authoredStaticSubMeshOOBBs = &authoredIt->second;

				if ( logCastleVillageWallColliderBuild )
				{
					size_t authoredBoxTotal = 0;
					for ( const auto& kv : authoredIt->second )
						authoredBoxTotal += kv.second.size();

					char buf[512];
					sprintf_s(
						buf,
						"[ColliderBuild][SCENE_AUTHORED_TABLE_FOUND] asset=\"%s\" object=\"%s\" pathGroupCount=%zu authoredBoxTotal=%zu\n",
						placement.assetName.c_str(),
						placement.objectName.c_str(),
						authoredIt->second.size(),
						authoredBoxTotal
					);
					OutputDebugStringA(buf);
				}
			}
			else if ( logCastleVillageWallColliderBuild )
			{
				char buf[512];
				sprintf_s(
					buf,
					"[ColliderBuild][SCENE_AUTHORED_TABLE_MISSING] asset=\"%s\" object=\"%s\"\n",
					placement.assetName.c_str(),
					placement.objectName.c_str()
				);
				OutputDebugStringA(buf);
			}
		}

		auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
		if ( !obj )
			continue;

		CGameObject* raw = obj.get();

#ifndef USING_NETWORK
		if ( placement.assetName == "Tower" )
		{
			RegisterTowerDoorPortal(raw);
		}
		else if ( placement.assetName == "Castle" )
		{
			RegisterCastleDoorPortal(raw);
		}
#endif

		const bool isTreeObject = ( resolvedAssetType == AssetType::Tree );
		const bool castsShadow = ShouldStaticPlacementCastShadow(placement.assetName);

		if ( isTreeObject )
		{
			m_treeAlphaClipObjects.insert(raw);
			m_staticTreeObjectIndices.push_back(i);
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
				lodEntry.lodDistance01 = 50.0f;
				lodEntry.lodDistance12 = 300.0f;
				lodEntry.cullDistance = 400.0f;
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
				lodEntry.cullDistance = 500.0f;
			}
			else if (
				placement.assetName == "Tower" )
			{
				lodEntry.lodDistance01 = 300.0f;
				lodEntry.lodDistance12 = 500.0f;
				lodEntry.cullDistance = 800.0f;
			}
			else if (
				placement.assetName == "Castle" )
			{
				lodEntry.lodDistance01 = 400.0f;
				lodEntry.lodDistance12 = 600.0f;
				lodEntry.cullDistance = 1000.0f;
			}

			else
			{
				lodEntry.lodDistance01 = 100.0f;
				lodEntry.lodDistance12 = 300.0f;
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

		RegisterStaticPlacementToGrid(placement, raw);

		const uint16_t collisionMegaGridMask =
			createWorldStaticCollider ? ComputeStaticObjectMegaGridMask(raw) : 0;

		SetObjectCollisionMegaGridMask(raw, collisionMegaGridMask, true);

		m_staticObjects.push_back(std::move(obj));
		b->objectRefs.push_back(raw);
		m_staticShadowCasterFlags.push_back(castsShadow ? 1 : 0);
		m_staticCollisionMegaGridMasks.push_back(collisionMegaGridMask);
		m_staticDynamicWorldMatrixFlags.push_back(0);
		b->count = ( UINT ) b->objectRefs.size();
	}

	// Terrain 연결
	/*for ( UINT k = 0; k < m_terrainCount; ++k )
	{
		if ( b->objectRefs.size() >= b->capacity ) break;

		const UINT i = ( UINT ) b->objectRefs.size();
		const StaticPlacementEntry& placement = m_staticPlacementEntries[k];

		const bool createWorldStaticCollider = false;
		const bool isStaticWorldLodTarget = false;
		const bool enableDistanceCull = false;

		std::shared_ptr<CMesh> selectedMesh = nullptr;

		if ( !selectedMesh )
		{


		}

		if ( !selectedMesh )
			continue;

		GameSceneObjectFactory::StaticRenderableDesc createDesc{};
		createDesc.ctx = MakeStaticContext(i);
		createDesc.mesh = selectedMesh;
		createDesc.position = placement.pos;
		createDesc.yawDeg = placement.yawDeg;

		createDesc.addCollider = false;
		const bool logCastleVillageWallColliderBuild = false;

		createDesc.debugColliderBuildLog = logCastleVillageWallColliderBuild;
		createDesc.debugColliderAssetName = placement.assetName;
		createDesc.debugColliderObjectName = placement.objectName;

		auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
		if ( !obj )
			continue;

		CGameObject* raw = obj.get();

		const bool castsShadow = false;

		RegisterStaticPlacementToGrid(placement, raw);

		const uint16_t collisionMegaGridMask =
			createWorldStaticCollider ? ComputeStaticObjectMegaGridMask(raw) : 0;

		if ( collisionMegaGridMask != 0 )
			m_collisionMegaGridMaskByObject[raw] = collisionMegaGridMask;

		m_staticObjects.push_back(std::move(obj));
		b->objectRefs.push_back(raw);
		m_staticShadowCasterFlags.push_back(castsShadow ? 1 : 0);
		m_staticCollisionMegaGridMasks.push_back(collisionMegaGridMask);
		b->count = ( UINT ) b->objectRefs.size();
	}*/

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

			createDesc.addAttackPower = true;
			createDesc.attackPower = 0;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			SetObjectCollisionMegaGridMask(raw, 0, false);
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(0);
			m_staticCollisionMegaGridMasks.push_back(0);
			m_staticDynamicWorldMatrixFlags.push_back(1);
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

			createDesc.addAttackPower = true;
			createDesc.attackPower = 0;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			SetObjectCollisionMegaGridMask(raw, 0, false);
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(0);
			m_staticCollisionMegaGridMasks.push_back(0);
			m_staticDynamicWorldMatrixFlags.push_back(1);
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
			SetObjectCollisionMegaGridMask(raw, 0, false);
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(0);
			m_staticCollisionMegaGridMasks.push_back(0);
			m_staticDynamicWorldMatrixFlags.push_back(1);
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

			createDesc.addPlayerWeaponHitbox = true;

			createDesc.addAttackPower = true;
			createDesc.attackPower = GetCurrentPlayerSwordAttackPower();

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			SetObjectCollisionMegaGridMask(raw, 0, false);
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(1);
			m_staticCollisionMegaGridMasks.push_back(0);
			m_staticDynamicWorldMatrixFlags.push_back(1);
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

			createDesc.addPlayerWeaponHitbox = true;

			createDesc.addAttackPower = true;
			createDesc.attackPower = GetCurrentPlayerAxeAttackPower();

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			SetObjectCollisionMegaGridMask(raw, 0, false);
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(1);
			m_staticCollisionMegaGridMasks.push_back(0);
			m_staticDynamicWorldMatrixFlags.push_back(1);
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
			SetObjectCollisionMegaGridMask(raw, 0, false);
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(1);
			m_staticCollisionMegaGridMasks.push_back(0);
			m_staticDynamicWorldMatrixFlags.push_back(1);
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
			createDesc.addAttackPower = true;
			createDesc.attackPower = kAttackPowerEnemySword;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			SetObjectCollisionMegaGridMask(raw, 0, false);
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(1);
			m_staticCollisionMegaGridMasks.push_back(0);
			m_staticDynamicWorldMatrixFlags.push_back(1);
			b->count = ( UINT ) b->objectRefs.size();

			m_EnemySwordRefs.push_back(raw);
		}
	}

	BuildStaticOcclusionEntries();

	m_staticShadowOcclusionEntryIndices.assign(m_staticBatch.objectRefs.size(), -1);

	for ( UINT entryIndex = 0; entryIndex < ( UINT ) m_staticOcclusionEntries.size(); ++entryIndex )
	{
		const StaticOcclusionEntry& entry = m_staticOcclusionEntries[entryIndex];

		if ( entry.staticBatchObjectIndex >= ( UINT ) m_staticShadowOcclusionEntryIndices.size() )
			continue;

		m_staticShadowOcclusionEntryIndices[entry.staticBatchObjectIndex] =
			static_cast< int >(entryIndex);
	}

	BuildStaticOcclusionUnitBoxMesh(dev, cmd);
	BuildStaticOcclusionGpuResources(dev);

	BuildStaticGameplayTickList();
	BuildStaticInstanceGroups();
	BuildStaticRenderObjectCache();

	m_staticTreeGridCullFlags.assign(m_staticBatch.objectRefs.size(), 0);

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

	if ( m_staticInstanceBufferCapacity > 0 )
	{
		// pass 0: scene
		// pass 1: shadow
		const UINT kStaticInstancePassCount = 2;

		const UINT instanceBufferBytes =
			sizeof(StaticInstanceVertex) *
			m_staticInstanceBufferCapacity *
			kStaticInstancePassCount;

		for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
		{
			m_pd3dStaticInstanceBuffer[frameIndex] = ::CreateBufferResource(
				dev,
				cmd,
				nullptr,
				instanceBufferBytes,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr
			);

			if ( m_pd3dStaticInstanceBuffer[frameIndex] )
			{
				m_pd3dStaticInstanceBuffer[frameIndex]->Map(
					0,
					nullptr,
					reinterpret_cast< void** >( &m_pMappedStaticInstanceBuffer[frameIndex] )
				);
			}
		}
	}
}

XMFLOAT3 CGameScene::ComputeEnemySpawnerSpawnPosition(
	int megaGridNumber,
	UINT localIndex,
	UINT localCount) const
{
	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return XMFLOAT3(0.0f, 0.0f, 0.0f);

	if ( localCount == 0 )
		localCount = 1;

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

	const UINT columns = std::max< UINT >(
		1,
		static_cast< UINT >( std::ceil(std::sqrt(static_cast< float >( localCount ))) )
	);

	const UINT rows = ( localCount + columns - 1 ) / columns;

	const UINT col = localIndex % columns;
	const UINT row = localIndex / columns;

	constexpr float kSpawnSpacing = 3.0f;

	const float totalWidth = static_cast< float >( columns > 0 ? columns - 1 : 0 ) * kSpawnSpacing;
	const float totalDepth = static_cast< float >( rows > 0 ? rows - 1 : 0 ) * kSpawnSpacing;

	XMFLOAT3 pos{};
	pos.x = centerX + static_cast< float >( col ) * kSpawnSpacing - totalWidth * 0.5f;
	pos.y = 0.0f;
	pos.z = centerZ + static_cast< float >( row ) * kSpawnSpacing - totalDepth * 0.5f;

	return pos;
}

XMFLOAT3 CGameScene::ComputeBossCallMonsterSpawnPosition() const
{
	// 5번 메가그리드 중심.
	constexpr int megaGridNumber = 5;

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

	// 200 x 200 내부 랜덤.
	constexpr float halfExtent = 100.0f;

	static std::mt19937 rng{ std::random_device{}( ) };
	std::uniform_real_distribution<float> dist(-halfExtent, halfExtent);

	XMFLOAT3 pos{};
	pos.x = centerX + dist(rng);
	pos.y = 0.0f;
	pos.z = centerZ + dist(rng);

	return pos;
}

float CGameScene::ComputeBossCallMonsterSpawnYawDeg() const
{
	static std::mt19937 rng{ std::random_device{}( ) };
	std::uniform_real_distribution<float> dist(-180.0f, 180.0f);

	return dist(rng);
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
	b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		b->cbGameObjects[frameIndex] = ::CreateBufferResource(
			dev, cmd, nullptr,
			b->cbElementBytes * cap,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			nullptr
		);

		if ( b->cbGameObjects[frameIndex] )
		{
			b->cbGameObjects[frameIndex]->Map(
				0,
				nullptr,
				reinterpret_cast< void** >( &b->mappedGameObjects[frameIndex] )
			);
		}

		b->baseCbvGpu[frameIndex] = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();

		m_pDescriptorHeap->CreateConstantBufferViews(
			dev,
			cap,
			b->cbGameObjects[frameIndex].Get(),
			b->cbElementBytes
		);
	}

	m_skinnedObjects.clear();

	m_skinnedAlphaClipObjects.clear();
	m_skinnedAlphaClipObjects.reserve(cap);

	m_skinnedObjects.reserve(cap);

	b->objectRefs.clear();
	b->objectRefs.reserve(cap);

	b->count = 0;

	ResetSkinnedWorldLodEntries();
	m_skinnedWorldLodEntries.reserve(cap);

	m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };

	auto MakeSkinnedContext = [ & ] (UINT objectIndex)
		{
			const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

			GameSceneObjectFactory::CreateContext ctx{};
			ctx.device = dev;
			ctx.cmd = cmd;
			ctx.mappedGameObjectCB =
				reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >( b->mappedGameObjects[frameIndex] ) +
					objectIndex * b->cbElementBytes
				);
			ctx.cbvGpuHandle.ptr =
				b->baseCbvGpu[frameIndex].ptr +
				static_cast< UINT64 >( objectIndex ) * b->cbvInc;
			return ctx;
		};

	enum class ELocalMonsterAIKind
	{
		Ghoul,
		EnemySpawnerGhoul,

		BossStageGhoul,
		BossStageSwordMan,
		BossStageBowMan,
		BossStageMutant,

		SwordMan,
		BowMan,
		Mutant,
		Boss
	};

	auto ShouldAttachLocalMonsterAI =
		[ this ] (ELocalMonsterAIKind kind) -> bool
		{
			if ( !m_bSimulateLocalAI )
				return false;

			switch ( kind )
			{
			case ELocalMonsterAIKind::Ghoul:
				return m_bSimulateLocalGhoulAI;

			case ELocalMonsterAIKind::EnemySpawnerGhoul:
				return m_bSimulateLocalEnemySpawner;

			case ELocalMonsterAIKind::BossStageGhoul:
			case ELocalMonsterAIKind::BossStageSwordMan:
			case ELocalMonsterAIKind::BossStageBowMan:
			case ELocalMonsterAIKind::BossStageMutant:
				return m_bSimulateLocalBossStageMonsterAI;

			case ELocalMonsterAIKind::BowMan:
				return m_bSimulateLocalBowManAI;

			case ELocalMonsterAIKind::SwordMan:
				return m_bSimulateLocalSwordManAI;

			case ELocalMonsterAIKind::Mutant:
				return m_bSimulateLocalMutantAI;

			case ELocalMonsterAIKind::Boss:
				return m_bSimulateLocalBossAI || m_bSimulateLocalBossSummon;

			default:
				break;
			}

			return false;
		};

	auto AttachMonsterAIToMonster =
		[ this, &ShouldAttachLocalMonsterAI ](
			std::unique_ptr<CGameObject>& obj,
			ELocalMonsterAIKind kind)
		{
			if ( !ShouldAttachLocalMonsterAI(kind) )
				return;

			if ( !obj )
				return;

			switch ( kind )
			{
			case ELocalMonsterAIKind::Ghoul:
			{
				if ( obj->GetComponent<CGhoulAIComponent>() )
					return;

				auto* ai = obj->AddComponent<CGhoulAIComponent>();
				if ( ai )
				{
					ai->SetScene(this);
					ai->SetEnabledAI(true);
				}
				break;
			}

			case ELocalMonsterAIKind::EnemySpawnerGhoul:
			{
				if ( obj->GetComponent<CEnemySpawnerGhoulAIComponent>() )
					return;

				auto* ai = obj->AddComponent<CEnemySpawnerGhoulAIComponent>();
				if ( ai )
				{
					ai->SetScene(this);
					ai->SetEnabledAI(true);
				}
				break;
			}

			case ELocalMonsterAIKind::BossStageGhoul:
			{
				if ( obj->GetComponent<CBossStageMonsterAIComponent>() )
					return;

				auto* ai = obj->AddComponent<CBossStageMonsterAIComponent>();
				if ( ai )
				{
					ai->SetScene(this);
					ai->ConfigureBossStageMonsterAI(
						CBossStageMonsterAIComponent::EKind::Ghoul
					);
					ai->SetEnabledAI(true);
				}
				break;
			}

			case ELocalMonsterAIKind::BossStageSwordMan:
			{
				if ( obj->GetComponent<CBossStageMonsterAIComponent>() )
					return;

				auto* ai = obj->AddComponent<CBossStageMonsterAIComponent>();
				if ( ai )
				{
					ai->SetScene(this);
					ai->ConfigureBossStageMonsterAI(
						CBossStageMonsterAIComponent::EKind::SwordMan
					);
					ai->SetEnabledAI(true);
				}
				break;
			}

			case ELocalMonsterAIKind::BossStageBowMan:
			{
				if ( obj->GetComponent<CBossStageMonsterAIComponent>() )
					return;

				auto* ai = obj->AddComponent<CBossStageMonsterAIComponent>();
				if ( ai )
				{
					ai->SetScene(this);
					ai->ConfigureBossStageMonsterAI(
						CBossStageMonsterAIComponent::EKind::BowMan
					);
					ai->SetEnabledAI(true);
				}
				break;
			}

			case ELocalMonsterAIKind::BossStageMutant:
			{
				if ( obj->GetComponent<CBossStageMonsterAIComponent>() )
					return;

				auto* ai = obj->AddComponent<CBossStageMonsterAIComponent>();
				if ( ai )
				{
					ai->SetScene(this);
					ai->ConfigureBossStageMonsterAI(
						CBossStageMonsterAIComponent::EKind::Mutant
					);
					ai->SetEnabledAI(true);
				}
				break;
			}

			case ELocalMonsterAIKind::SwordMan:
			{
				if ( obj->GetComponent<CSwordManAIComponent>() )
					return;

				auto* ai = obj->AddComponent<CSwordManAIComponent>();
				if ( ai )
				{
					ai->SetScene(this);
					ai->SetEnabledAI(true);
				}
				break;
			}

			case ELocalMonsterAIKind::BowMan:
			{
				if ( obj->GetComponent<CBowManAIComponent>() )
					return;

				auto* ai = obj->AddComponent<CBowManAIComponent>();
				if ( ai )
				{
					ai->SetScene(this);
					ai->SetEnabledAI(true);
				}
				break;
			}

			case ELocalMonsterAIKind::Mutant:
			{
				if ( obj->GetComponent<CMutantAIComponent>() )
					return;

				auto* ai = obj->AddComponent<CMutantAIComponent>();
				if ( ai )
				{
					ai->SetScene(this);
					ai->SetEnabledAI(true);
				}
				break;
			}

			case ELocalMonsterAIKind::Boss:
			{
				if ( obj->GetComponent<CBossAIComponent>() )
					return;

				auto* ai = obj->AddComponent<CBossAIComponent>();
				if ( ai )
				{
					const bool bossCombatAIEnabled = m_bSimulateLocalBossAI;
					const bool bossSummonEnabled =
						m_bSimulateLocalBossSummon || bossCombatAIEnabled;

					ai->SetScene(this);
					ai->ConfigureBossSimulation(
						bossCombatAIEnabled,
						bossSummonEnabled
					);
					ai->SetEnabledAI(
						bossCombatAIEnabled || bossSummonEnabled
					);
				}
				break;
			}

			default:
				break;
			}
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

	m_ghoulRefs.clear();
	m_ghoulRefs.reserve(m_ghoulCount);

	m_swordManRefs.clear();
	m_swordManRefs.reserve(m_swordManCount);

	m_bowManRefs.clear();
	m_bowManRefs.reserve(m_bowManCount);

	m_MutantRefs.clear();
	m_MutantRefs.reserve(m_MutantCount);

	m_EnemySpawnRefs.clear();
	m_EnemySpawnRefs.reserve(m_EnemySpawnCount);

	m_enemySpawnPoolEntries.clear();
	m_enemySpawnPoolEntries.reserve(m_EnemySpawnCount);

	m_bossRefs.clear();
	m_bossRefs.reserve(m_bossCount);

	m_mutantKeyTriggerMegaByObject.clear();
	m_mutantKeyTriggerRegisteredByMega.fill(false);

	m_skinnedMonsterMegaGridNumbers.clear();
	m_skinnedMonsterMegaGridNumbers.reserve(m_skinnedBatch.capacity);
	m_sceneGrid.ClearMegaGridMonsters();

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
	auto RegisterEnemySpawnerPoolObject =
		[ this ](
			CGameObject* raw,
			EEnemySpawnerEnemyKind kind,
			int megaGridNumber,
			const XMFLOAT3& spawnPosition)
		{
			if ( !raw )
				return;

			XMFLOAT3 inactivePosition = spawnPosition;
			inactivePosition.y = kEnemySpawnerInactiveY;

			raw->SetActive(false);
			raw->SetPosition(inactivePosition);

			if ( auto* collider = raw->GetComponent<CColliderComponent>() )
			{
				collider->SetEnabled(false);
				collider->UpdateWorldBounds();
			}

			m_EnemySpawnRefs.push_back(raw);

			EnemySpawnerPoolEntry entry{};
			entry.object = raw;
			entry.kind = kind;
			entry.megaGridNumber = megaGridNumber;

			// 중요:
			// entry.spawnPosition은 실제 활성화 위치로 남겨야 한다.
			// 여기를 inactivePosition으로 넣으면 SpawnMegaGrid() 때 -100에서 생성된다.
			entry.spawnPosition = spawnPosition;

			m_enemySpawnPoolEntries.push_back(entry);
		};

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

			createDesc.addHealth = true;
			createDesc.maxHp = kHpGhoul;

			createDesc.addAttackPower = true;
			createDesc.attackPower = kAttackPowerGhoul;

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

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

#ifndef USING_NETWORK
			const int spawnMegaGridNumber =
				m_sceneGrid.MegaGridNumberFromWorldPosition(pos.x, pos.z);

			AttachMonsterAIToMonster(
				obj,
				( spawnMegaGridNumber == 5 )
					? ELocalMonsterAIKind::BossStageGhoul
					: ELocalMonsterAIKind::Ghoul
			);
#endif

			++enemyIndex;

			CGameObject* raw = obj.get();

			RegisterMonsterToMegaGrid(raw, pos, i);

			RegisterSkinnedCullEntry(
				raw, i, "Ghoul", pos,
				ghoulLodMeshes, true,
				35.0f, 90.0f, 120.0f
			);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_ghoulRefs.push_back(raw);
		}
#ifndef USING_NETWORK
		auto CreateEnemySpawnGhoulPool =
			[ & ] (int megaGridNumber, UINT count)
			{
				for ( UINT k = 0; k < count; ++k )
				{
					if ( b->objectRefs.size() >= b->capacity )
						break;

					const UINT i = static_cast< UINT >(b->objectRefs.size());

					const XMFLOAT3 pos =
						( megaGridNumber == 5 )
						? ComputeBossCallMonsterSpawnPosition()
						: ComputeEnemySpawnerSpawnPosition(megaGridNumber, k, count);

					const float yaw =
						( megaGridNumber == 5 )
						? ComputeBossCallMonsterSpawnYawDeg()
						: 180.0f;

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

					createDesc.addHealth = true;
					createDesc.maxHp = kHpGhoul;

					createDesc.addAttackPower = true;
					createDesc.attackPower = kAttackPowerGhoul;

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

					auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
					if ( !obj )
						continue;

					const bool useSpawnerRushGhoulAI =
						( megaGridNumber == 6 || megaGridNumber == 8 );

					const ELocalMonsterAIKind ghoulAIKind =
						( megaGridNumber == 5 )
						? ELocalMonsterAIKind::BossStageGhoul
						: (
							useSpawnerRushGhoulAI
								? ELocalMonsterAIKind::EnemySpawnerGhoul
								: ELocalMonsterAIKind::Ghoul
						);

					AttachMonsterAIToMonster(obj, ghoulAIKind);

					CGameObject* raw = obj.get();

					if ( useSpawnerRushGhoulAI )
					{
						if ( auto* ai = raw->GetComponent<CEnemySpawnerGhoulAIComponent>() )
						{
							ai->ConfigureSpawnerGhoulAI(megaGridNumber, 60.0f);
						}
					}

					RegisterMonsterToMegaGrid(raw, pos, i);

					RegisterSkinnedCullEntry(
						raw, i, "Ghoul", pos,
						ghoulLodMeshes, true,
						35.0f, 90.0f, 120.0f
					);

					RegisterEnemySpawnerPoolObject(
						raw,
						EEnemySpawnerEnemyKind::Ghoul,
						megaGridNumber,
						pos
					);

					m_skinnedObjects.push_back(std::move(obj));
					b->objectRefs.push_back(raw);
					b->count = static_cast< UINT >( b->objectRefs.size() );

					m_ghoulRefs.push_back(raw);
				}
			};

		CreateEnemySpawnGhoulPool(6, kEnemySpawnerMega6GhoulCount);
		CreateEnemySpawnGhoulPool(8, kEnemySpawnerMega8GhoulCount);
		CreateEnemySpawnGhoulPool(5, kEnemySpawnerMega5GhoulCount);
#endif
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

			createDesc.addHealth = true;
			createDesc.maxHp = kHpSwordMan;

			createDesc.skeletonKey = "EnemySword";
			createDesc.clipEntries = &swordClips;

			createDesc.initMonsterController = true;
			createDesc.monsterInitialState = EMonsterAnimState::Idle;
			createDesc.monsterProfile.idleClip = "Idle";
			createDesc.monsterProfile.moveClip = "Walk";
			createDesc.monsterProfile.runClip = "Run";
			createDesc.monsterProfile.hitClip = "Hit";
			createDesc.monsterProfile.attackClip = "Attack";
			createDesc.monsterProfile.deathClip = "Death";

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

#ifndef USING_NETWORK
			const int spawnMegaGridNumber =
				m_sceneGrid.MegaGridNumberFromWorldPosition(pos.x, pos.z);

			AttachMonsterAIToMonster(
				obj,
				( spawnMegaGridNumber == 5 )
					? ELocalMonsterAIKind::BossStageSwordMan
					: ELocalMonsterAIKind::SwordMan
			);
#endif
			++enemyIndex;

			CGameObject* raw = obj.get();

			RegisterMonsterToMegaGrid(raw, pos, i);

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
#ifndef USING_NETWORK
		auto CreateEnemySpawnSwordManPool =
			[ & ] (int megaGridNumber, UINT count)
			{
				for ( UINT k = 0; k < count; ++k )
				{
					if ( b->objectRefs.size() >= b->capacity )
						break;

					const UINT i = static_cast< UINT >(b->objectRefs.size());

					const XMFLOAT3 pos =
						( megaGridNumber == 5 )
						? ComputeBossCallMonsterSpawnPosition()
						: ComputeEnemySpawnerSpawnPosition(megaGridNumber, k, count);

					const float yaw =
						( megaGridNumber == 5 )
						? ComputeBossCallMonsterSpawnYawDeg()
						: 180.0f;

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

					createDesc.addHealth = true;
					createDesc.maxHp = kHpSwordMan;

					createDesc.skeletonKey = "EnemySword";
					createDesc.clipEntries = &swordClips;

					createDesc.initMonsterController = true;
					createDesc.monsterInitialState = EMonsterAnimState::Idle;
					createDesc.monsterProfile.idleClip = "Idle";
					createDesc.monsterProfile.moveClip = "Walk";
					createDesc.monsterProfile.runClip = "Run";
					createDesc.monsterProfile.hitClip = "Hit";
					createDesc.monsterProfile.attackClip = "Attack";
					createDesc.monsterProfile.deathClip = "Death";

					auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
					if ( !obj )
						continue;

					AttachMonsterAIToMonster(
						obj,
						( megaGridNumber == 5 )
							? ELocalMonsterAIKind::BossStageSwordMan
							: ELocalMonsterAIKind::SwordMan
					);
					CGameObject* raw = obj.get();

					RegisterMonsterToMegaGrid(raw, pos, i);

					std::array<std::shared_ptr<CMesh>, 3> noLodMeshes =
					{
						swordManAsset.mesh, nullptr, nullptr
					};

					RegisterSkinnedCullEntry(
						raw, i, "SwordMan", pos,
						noLodMeshes, false,
						0.0f, 0.0f, 90.0f
					);

					RegisterEnemySpawnerPoolObject(
						raw,
						EEnemySpawnerEnemyKind::SwordMan,
						megaGridNumber,
						pos
					);

					m_skinnedObjects.push_back(std::move(obj));
					b->objectRefs.push_back(raw);
					b->count = static_cast< UINT >( b->objectRefs.size() );

					m_swordManRefs.push_back(raw);
				}
			};

		CreateEnemySpawnSwordManPool(5, kEnemySpawnerMega5SwordManCount);
#endif
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

			createDesc.addHealth = true;
			createDesc.maxHp = kHpBowMan;

			createDesc.skeletonKey = "EnemyBow";
			createDesc.clipEntries = &bowManClips;

			createDesc.initMonsterController = true;
			createDesc.monsterInitialState = EMonsterAnimState::Idle;
			createDesc.monsterProfile.idleClip = "Idle";
			createDesc.monsterProfile.moveClip = "Walk";
			createDesc.monsterProfile.runClip = "Run";
			createDesc.monsterProfile.hitClip = "Hit";
			createDesc.monsterProfile.deathClip = "Death";
			createDesc.monsterProfile.attackClip = "Bow_Load";
			createDesc.monsterProfile.attackNextClip = "Bow_Release";
			createDesc.monsterProfile.attackHasChain = true;

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

#ifndef USING_NETWORK
			const int spawnMegaGridNumber =
				m_sceneGrid.MegaGridNumberFromWorldPosition(pos.x, pos.z);

			AttachMonsterAIToMonster(
				obj,
				( spawnMegaGridNumber == 5 )
					? ELocalMonsterAIKind::BossStageBowMan
					: ELocalMonsterAIKind::BowMan
			);
#endif

			++enemyIndex;

			CGameObject* raw = obj.get();

			RegisterMonsterToMegaGrid(raw, pos, i);

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
#ifndef USING_NETWORK
		auto CreateEnemySpawnBowManPool =
			[ & ] (int megaGridNumber, UINT count)
			{
				for ( UINT k = 0; k < count; ++k )
				{
					if ( b->objectRefs.size() >= b->capacity )
						break;

					const UINT i = static_cast< UINT >(b->objectRefs.size());

					const XMFLOAT3 pos =
						( megaGridNumber == 5 )
						? ComputeBossCallMonsterSpawnPosition()
						: ComputeEnemySpawnerSpawnPosition(megaGridNumber, k, count);

					const float yaw =
						( megaGridNumber == 5 )
						? ComputeBossCallMonsterSpawnYawDeg()
						: 180.0f;

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

					createDesc.addHealth = true;
					createDesc.maxHp = kHpBowMan;

					createDesc.skeletonKey = "EnemyBow";
					createDesc.clipEntries = &bowManClips;

					createDesc.initMonsterController = true;
					createDesc.monsterInitialState = EMonsterAnimState::Idle;
					createDesc.monsterProfile.idleClip = "Idle";
					createDesc.monsterProfile.moveClip = "Walk";
					createDesc.monsterProfile.runClip = "Run";
					createDesc.monsterProfile.hitClip = "Hit";
					createDesc.monsterProfile.deathClip = "Death";
					createDesc.monsterProfile.attackClip = "Bow_Load";
					createDesc.monsterProfile.attackNextClip = "Bow_Release";
					createDesc.monsterProfile.attackHasChain = true;

					auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
					if ( !obj )
						continue;

					AttachMonsterAIToMonster(
						obj,
						( megaGridNumber == 5 )
							? ELocalMonsterAIKind::BossStageBowMan
							: ELocalMonsterAIKind::BowMan
					);

					CGameObject* raw = obj.get();

					RegisterMonsterToMegaGrid(raw, pos, i);

					std::array<std::shared_ptr<CMesh>, 3> noLodMeshes =
					{
						bowManAsset.mesh, nullptr, nullptr
					};

					RegisterSkinnedCullEntry(
						raw, i, "BowMan", pos,
						noLodMeshes, false,
						0.0f, 0.0f, 100.0f
					);

					RegisterEnemySpawnerPoolObject(
						raw,
						EEnemySpawnerEnemyKind::BowMan,
						megaGridNumber,
						pos
					);

					m_skinnedObjects.push_back(std::move(obj));
					b->objectRefs.push_back(raw);
					b->count = static_cast< UINT >( b->objectRefs.size() );

					m_bowManRefs.push_back(raw);
				}
			};

		CreateEnemySpawnBowManPool(5, kEnemySpawnerMega5BowManCount);
#endif
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

			createDesc.addHealth = true;
			createDesc.maxHp = kHpMutant;

			createDesc.addAttackPower = true;
			createDesc.attackPower = kAttackPowerMutant;

			createDesc.skeletonKey = "Mutant";
			createDesc.clipEntries = &mutantClips;

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
				{ "Attack", 0.20f, 0.55f, { "CATRigRArmPalm" } }
			);

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

#ifndef USING_NETWORK
			const int spawnMegaGridNumber =
				m_sceneGrid.MegaGridNumberFromWorldPosition(pos.x, pos.z);

			AttachMonsterAIToMonster(
				obj,
				( spawnMegaGridNumber == 5 )
					? ELocalMonsterAIKind::BossStageMutant
					: ELocalMonsterAIKind::Mutant
			);
#endif

			++enemyIndex;

			CGameObject* raw = obj.get();

			RegisterMonsterToMegaGrid(raw, pos, i);

			const int mutantMegaGridNumber =
				m_sceneGrid.MegaGridNumberFromWorldPosition(pos.x, pos.z);

			RegisterMutantKeyTriggerIfNeeded(raw, mutantMegaGridNumber);

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
#ifndef USING_NETWORK
		auto CreateEnemySpawnMutantPool =
			[ & ] (int megaGridNumber, UINT count)
			{
				for ( UINT k = 0; k < count; ++k )
				{
					if ( b->objectRefs.size() >= b->capacity )
						break;

					const UINT i = static_cast< UINT >(b->objectRefs.size());

					const XMFLOAT3 pos =
						( megaGridNumber == 5 )
						? ComputeBossCallMonsterSpawnPosition()
						: ComputeEnemySpawnerSpawnPosition(megaGridNumber, k, count);

					const float yaw =
						( megaGridNumber == 5 )
						? ComputeBossCallMonsterSpawnYawDeg()
						: 180.0f;

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

					createDesc.addHealth = true;
					createDesc.maxHp = kHpMutant;

					createDesc.addAttackPower = true;
					createDesc.attackPower = kAttackPowerMutant;

					createDesc.skeletonKey = "Mutant";
					createDesc.clipEntries = &mutantClips;

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
						{ "Attack", 0.20f, 0.55f, { "CATRigRArmPalm" } }
					);

					auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
					if ( !obj )
						continue;

					AttachMonsterAIToMonster(
						obj,
						( megaGridNumber == 5 )
							? ELocalMonsterAIKind::BossStageMutant
							: ELocalMonsterAIKind::Mutant
					);
					CGameObject* raw = obj.get();

					RegisterMonsterToMegaGrid(raw, pos, i);

					std::array<std::shared_ptr<CMesh>, 3> noLodMeshes =
					{
						mutantAsset.mesh, nullptr, nullptr
					};

					RegisterSkinnedCullEntry(
						raw, i, "Mutant", pos,
						noLodMeshes, false,
						0.0f, 0.0f, 110.0f
					);

					RegisterEnemySpawnerPoolObject(
						raw,
						EEnemySpawnerEnemyKind::Mutant,
						megaGridNumber,
						pos
					);

					m_skinnedObjects.push_back(std::move(obj));
					b->objectRefs.push_back(raw);
					b->count = static_cast< UINT >( b->objectRefs.size() );

					m_MutantRefs.push_back(raw);
				}
			};

		CreateEnemySpawnMutantPool(5, kEnemySpawnerMega5MutantCount);
#endif
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

			createDesc.addHealth = true;
			createDesc.maxHp = kHpBoss;

			createDesc.addAttackPower = true;
			createDesc.attackPower = kAttackPowerBoss;

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

#ifndef USING_NETWORK
			AttachMonsterAIToMonster(obj, ELocalMonsterAIKind::Boss);
#endif

			++enemyIndex;

			CGameObject* raw = obj.get();

			m_bossRefs.push_back(raw);

#ifndef USING_NETWORK
			// 보스는 처음부터 실제 스폰 위치가 아니라 지하에 숨겨 둔다.
			// x/z는 유지하고 y만 -100 정도 내려서, 혹시 1프레임 렌더되어도 화면에 보이지 않게 한다.
			RegisterBossStageBossOriginalPosition(raw, pos);
			MoveBossStageBossToHiddenPosition(raw);

			SetBossStageBossActive(raw, false, false);
#endif

			RegisterMonsterToMegaGrid(raw, pos, i); 

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

			createDesc.addHealth = true;
			createDesc.maxHp = kHpPlayer;

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
			if ( auto* equip = raw->GetComponent<CPlayerEquipmentComponent>() )
			{
				equip->SetAudioManager(m_pAudioManager);
			}

			if ( slot >= 0 && slot <= 3 )
				m_playersBySlot[( size_t ) slot] = raw;

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			SetObjectCollisionMegaGridMask(raw, 0, false);
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
			m_skinnedAlphaClipObjects.insert(raw);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			SetObjectCollisionMegaGridMask(raw, 0, false);
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
			m_skinnedAlphaClipObjects.insert(raw);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			SetObjectCollisionMegaGridMask(raw, 0, false);
			b->count = ( UINT ) b->objectRefs.size();

			m_EnemyBowRefs.push_back(raw);
		}
	}

	m_preparedBowmanArrows.assign(m_bowManRefs.size(), nullptr);
	m_prevEnemyBowReleasePhase.assign(m_bowManRefs.size(), false);

	m_prevGhoulAttackPhase.assign(m_ghoulRefs.size(), false);
	m_prevSwordManAttackPhase.assign(m_swordManRefs.size(), false);
	m_prevMutantAttackPhase.assign(m_MutantRefs.size(), false);
	m_prevBowManSfxLoadPhase.assign(m_bowManRefs.size(), false);

	m_monsterFootstepSfxStates.assign(
		m_ghoulRefs.size() +
		m_swordManRefs.size() +
		m_bowManRefs.size() +
		m_MutantRefs.size(),
		MonsterFootstepSfxState{}
	);

	m_pendingMonsterSfxList.clear();
	m_activeMonsterSfxList.clear();
	BuildSkinnedComponentCache();
	BuildSkinnedInstanceGroups();

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

	const UINT skinnedObjectCount =
		static_cast< UINT >( m_skinnedBatch.objectRefs.size() );

	m_skinnedBonePaletteBaseByObject.resize(skinnedObjectCount, 0);
	m_skinnedBonePaletteCountByObject.resize(skinnedObjectCount, 0);

	UINT runningBonePaletteBase = 0;

	for ( UINT i = 0; i < skinnedObjectCount; ++i )
	{
		const SkinnedComponentCache* cache = GetSkinnedComponentCache(i);

		UINT boneCount = 1;

		if ( cache && cache->object )
		{
			if ( cache->skinning )
			{
				boneCount = static_cast< UINT >(cache->skinning->GetBoneCount());
			}
			else
			{
				boneCount = static_cast< UINT >( cache->object->GetBoneCount() );
			}

			if ( boneCount == 0 )
				boneCount = 1;
		}

		m_skinnedBonePaletteBaseByObject[i] = runningBonePaletteBase;
		m_skinnedBonePaletteCountByObject[i] = boneCount;

		runningBonePaletteBase += boneCount;
	}

	m_skinnedBonePaletteCapacity = runningBonePaletteBase;

	if ( m_skinnedInstanceBufferCapacity > 0 )
	{
		// pass 0: scene
		// pass 1: shadow
		const UINT kSkinnedInstancePassCount = 2;

		const UINT instanceBufferBytes =
			sizeof(SkinnedInstanceVertex) *
			m_skinnedInstanceBufferCapacity *
			kSkinnedInstancePassCount;

		for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
		{
			m_pd3dSkinnedInstanceBuffer[frameIndex] = ::CreateBufferResource(
				dev,
				cmd,
				nullptr,
				instanceBufferBytes,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr
			);

			if ( m_pd3dSkinnedInstanceBuffer[frameIndex] )
			{
				m_pd3dSkinnedInstanceBuffer[frameIndex]->Map(
					0,
					nullptr,
					reinterpret_cast< void** >( &m_pMappedSkinnedInstanceBuffer[frameIndex] )
				);
			}
		}
	}

	if ( m_skinnedBonePaletteCapacity > 0 )
	{
		const UINT bonePaletteBufferBytes =
			sizeof(XMFLOAT4X4) * m_skinnedBonePaletteCapacity;

		for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
		{
			m_pd3dSkinnedBonePaletteBuffer[frameIndex] = ::CreateBufferResource(
				dev,
				cmd,
				nullptr,
				bonePaletteBufferBytes,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr
			);

			if ( m_pd3dSkinnedBonePaletteBuffer[frameIndex] )
			{
				m_pd3dSkinnedBonePaletteBuffer[frameIndex]->Map(
					0,
					nullptr,
					reinterpret_cast< void** >( &m_pMappedSkinnedBonePaletteBuffer[frameIndex] )
				);
			}
		}
	}

	BuildSkinnedOcclusionEntries();

	m_skinnedShadowOcclusionEntryIndices.assign(m_skinnedBatch.objectRefs.size(), -1);

	for ( UINT entryIndex = 0; entryIndex < ( UINT ) m_skinnedOcclusionEntries.size(); ++entryIndex )
	{
		const SkinnedOcclusionEntry& entry = m_skinnedOcclusionEntries[entryIndex];

		if ( entry.skinnedBatchObjectIndex >= ( UINT ) m_skinnedShadowOcclusionEntryIndices.size() )
			continue;

		m_skinnedShadowOcclusionEntryIndices[entry.skinnedBatchObjectIndex] =
			static_cast< int >(entryIndex);
	}

	BuildSkinnedOcclusionGpuResources(dev);
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
	b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		b->cbGameObjects[frameIndex] = ::CreateBufferResource(
			dev, cmd, nullptr,
			b->cbElementBytes * cap,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			nullptr
		);

		if ( b->cbGameObjects[frameIndex] )
		{
			b->cbGameObjects[frameIndex]->Map(
				0,
				nullptr,
				reinterpret_cast< void** >( &b->mappedGameObjects[frameIndex] )
			);
		}

		b->baseCbvGpu[frameIndex] = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();

		m_pDescriptorHeap->CreateConstantBufferViews(
			dev,
			cap,
			b->cbGameObjects[frameIndex].Get(),
			b->cbElementBytes
		);
	}

	m_staticObjects.clear();
	m_staticObjects.reserve(cap);

	b->objectRefs.clear();
	b->objectRefs.reserve(cap);

	m_colliderObjects.clear();
	m_colliderObjects.reserve(cap);

	b->count = 0;
	m_ColliderCount = 0;
}

void CGameScene::ResetStaticPlacementCounts()
{
	m_grassCount = 0;
	m_groundCount = 0;
	m_villagewallCount = 0;
	m_castleCount = 0;
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

	for ( const auto& e : m_staticPlacementEntries )
	{
		if ( e.assetName == "Grass" )       ++m_grassCount;
		else if ( e.assetName == "Ground" )      ++m_groundCount;
		else if ( e.assetName == "VillageWall" ) ++m_villagewallCount;
		else if ( e.assetName == "Castle" )    ++m_castleCount;
		else if ( e.assetName == "DirtRoad" )    ++m_dirtRoadCount;
		else if ( e.assetName == "Building1" )   ++m_building1Count;
		else if ( e.assetName == "Building2" )   ++m_building2Count;
		else if ( e.assetName == "Building3" )   ++m_building3Count;
		else if ( e.assetName == "Building4" )   ++m_building4Count;
		else if ( e.assetName == "Building5" )   ++m_building5Count;
		else if ( e.assetName == "Building6" )   ++m_building6Count;
		else if ( e.assetName == "Building7" )   ++m_building7Count;
		else if ( e.assetName == "Building8" )   ++m_building8Count;
		else if ( e.assetName == "Building9" )   ++m_building9Count;
		else if ( e.assetName == "Tower" )       ++m_towerCount;
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
	if ( !fin.is_open() )
		return false;

	std::string line;
	while ( std::getline(fin, line) )
	{
		if ( !line.empty() && line.back() == '\r' )
			line.pop_back();

		if ( line.rfind("ENTRY|", 0) != 0 )
			continue;

		StaticPlacementEntry entry{};
		if ( !ParsePlacementEntryLine(line, entry) )
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

void CGameScene::BuildStaticWorldSubmeshOOBBDebugObjects(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd)
{
	if ( !dev || !cmd ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	if ( !m_colliderBatch.mappedGameObjects[frameIndex] ) return;

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
					reinterpret_cast< UINT8* >( m_colliderBatch.mappedGameObjects[frameIndex] ) +
					i * m_colliderBatch.cbElementBytes
				);

				debugObj->SetCbvGPUDescriptorHandlePtr(
					m_colliderBatch.baseCbvGpu[frameIndex].ptr +
					static_cast< UINT64 >( i ) * m_colliderBatch.cbvInc
				);

				debugObj->SetMappedGameObjectCB(cb);

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
	m_lightObjects.reserve(1);
	m_pPlayerSpotFollower = nullptr;
	m_bMegaGrid5DirectionalLightProfileActive = false;

	// [0] Directional Light only
	{
		auto obj = std::make_unique<CGameObject>(0);

		if ( auto* tr = obj->GetComponent<CTransformComponent>() )
		{
			// 오른쪽 위 앞쪽에서 왼쪽 아래 뒤쪽으로 비추는 방향광
			tr->SetLookDirection(XMFLOAT3(1.0f, -1.0f, 0.3f));
		}

		auto* lc = obj->AddComponent<CLightComponent>();
		lc->type = ELightType::Directional;

		// 전역 환경광은 LIGHTS::m_xmf4GlobalAmbient에서 따로 넣고 있으므로
		// 여기 directional ambient는 0으로 유지.
		lc->ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		// 실질적으로 의미 있는 방향광.
		lc->diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		// 스펙큘러가 필요 없으면 0 유지.
		lc->specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		m_lightObjects.push_back(std::move(obj));
	}

	m_pMaterials = make_unique<MATERIALS>();
	::ZeroMemory(m_pMaterials.get(), sizeof(MATERIALS));

	AssetManager::BeginSceneMaterialBuild(m_pMaterials.get());

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

	for ( int i = 0; i < MAX_MATERIALS; ++i )
		m_pMaterials->m_pReflections[i].m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);

	{
		MATERIAL& keyMat = m_pMaterials->m_pReflections[kItemBillboardKeyMaterialId];

		keyMat.m_xmf4Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		keyMat.m_xmf4Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		keyMat.m_xmf4Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		keyMat.m_xmf4Emissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		keyMat.m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);

		keyMat.m_xmf4DiffuseUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		keyMat.m_xmf4NormalUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		keyMat.m_xmf4EmissiveUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		keyMat.m_xmf4SpecularUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);

		keyMat.m_xmn4WrapModes0 = XMUINT4(0, 0, 0, 0);
		keyMat.m_xmn4WrapModes1 = XMUINT4(0, 0, 0, 0);
	}
	{
		MATERIAL& transparentMat =
			m_pMaterials->m_pReflections[kTransparentItemBillboardMaterialId];

		transparentMat.m_xmf4Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		transparentMat.m_xmf4Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		transparentMat.m_xmf4Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		transparentMat.m_xmf4Emissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		transparentMat.m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);

		transparentMat.m_xmf4DiffuseUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		transparentMat.m_xmf4NormalUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		transparentMat.m_xmf4EmissiveUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		transparentMat.m_xmf4SpecularUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);

		transparentMat.m_xmn4WrapModes0 = XMUINT4(0, 0, 0, 0);
		transparentMat.m_xmn4WrapModes1 = XMUINT4(0, 0, 0, 0);
	}

	{
		MATERIAL& bossSummonMat =
			m_pMaterials->m_pReflections[kBossSummonCircleMaterialId];

		bossSummonMat.m_xmf4Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		bossSummonMat.m_xmf4Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
		bossSummonMat.m_xmf4Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		bossSummonMat.m_xmf4Emissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		bossSummonMat.m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);

		bossSummonMat.m_xmf4DiffuseUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossSummonMat.m_xmf4NormalUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossSummonMat.m_xmf4EmissiveUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossSummonMat.m_xmf4SpecularUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);

		bossSummonMat.m_xmn4WrapModes0 = XMUINT4(0, 0, 0, 0);
		bossSummonMat.m_xmn4WrapModes1 = XMUINT4(0, 0, 0, 0);
	}

	{
		MATERIAL& bossCallSummonMat =
			m_pMaterials->m_pReflections[kBossCallSummonCircleMaterialId];

		bossCallSummonMat.m_xmf4Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		bossCallSummonMat.m_xmf4Diffuse = XMFLOAT4(0.70f, 1.00f, 0.72f, 0.0f);
		bossCallSummonMat.m_xmf4Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		bossCallSummonMat.m_xmf4Emissive = XMFLOAT4(0.70f, 1.00f, 0.72f, 1.0f);

		bossCallSummonMat.m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);

		bossCallSummonMat.m_xmf4DiffuseUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossCallSummonMat.m_xmf4NormalUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossCallSummonMat.m_xmf4EmissiveUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossCallSummonMat.m_xmf4SpecularUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);

		bossCallSummonMat.m_xmn4WrapModes0 = XMUINT4(0, 0, 0, 0);
		bossCallSummonMat.m_xmn4WrapModes1 = XMUINT4(0, 0, 0, 0);
	}

	{
		MATERIAL& bossSummonGlowMat =
			m_pMaterials->m_pReflections[kBossSummonGlowMaterialId];

		bossSummonGlowMat.m_xmf4Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		bossSummonGlowMat.m_xmf4Diffuse = XMFLOAT4(0.70f, 1.00f, 0.72f, 0.0f);
		bossSummonGlowMat.m_xmf4Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		bossSummonGlowMat.m_xmf4Emissive = XMFLOAT4(0.70f, 1.00f, 0.72f, 1.0f);

		bossSummonGlowMat.m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);

		bossSummonGlowMat.m_xmf4DiffuseUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossSummonGlowMat.m_xmf4NormalUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossSummonGlowMat.m_xmf4EmissiveUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossSummonGlowMat.m_xmf4SpecularUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);

		bossSummonGlowMat.m_xmn4WrapModes0 = XMUINT4(0, 0, 0, 0);
		bossSummonGlowMat.m_xmn4WrapModes1 = XMUINT4(0, 0, 0, 0);
	}

	{
		MATERIAL& bossShockwaveMat =
			m_pMaterials->m_pReflections[kBossShockwaveMaterialId];

		bossShockwaveMat.m_xmf4Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		// 흙먼지/지면 바람 느낌의 갈색-회색
		bossShockwaveMat.m_xmf4Diffuse = XMFLOAT4(0.46f, 0.42f, 0.36f, 0.0f);
		bossShockwaveMat.m_xmf4Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		bossShockwaveMat.m_xmf4Emissive = XMFLOAT4(0.06f, 0.05f, 0.04f, 1.0f);

		bossShockwaveMat.m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);

		bossShockwaveMat.m_xmf4DiffuseUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossShockwaveMat.m_xmf4NormalUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossShockwaveMat.m_xmf4EmissiveUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossShockwaveMat.m_xmf4SpecularUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);

		bossShockwaveMat.m_xmn4WrapModes0 = XMUINT4(0, 0, 0, 0);
		bossShockwaveMat.m_xmn4WrapModes1 = XMUINT4(0, 0, 0, 0);
	}

	{
		MATERIAL& bossShockwaveWallMat =
			m_pMaterials->m_pReflections[kBossShockwaveWallMaterialId];

		bossShockwaveWallMat.m_xmf4Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		// 바닥보다 약간 밝은 회갈색 먼지벽
		bossShockwaveWallMat.m_xmf4Diffuse = XMFLOAT4(0.58f, 0.56f, 0.52f, 0.0f);
		bossShockwaveWallMat.m_xmf4Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		bossShockwaveWallMat.m_xmf4Emissive = XMFLOAT4(0.03f, 0.03f, 0.03f, 1.0f);

		bossShockwaveWallMat.m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);

		bossShockwaveWallMat.m_xmf4DiffuseUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossShockwaveWallMat.m_xmf4NormalUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossShockwaveWallMat.m_xmf4EmissiveUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		bossShockwaveWallMat.m_xmf4SpecularUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);

		bossShockwaveWallMat.m_xmn4WrapModes0 = XMUINT4(0, 0, 0, 0);
		bossShockwaveWallMat.m_xmn4WrapModes1 = XMUINT4(0, 0, 0, 0);
	}
}

void CGameScene::CreateShaderVariables(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
	m_nLightsCBElementBytes = ( ( sizeof(LIGHTS) + 255 ) & ~255 );

	for ( UINT i = 0; i < kFrameResourceCount; ++i )
	{
		m_pd3dcbLights[i] = ::CreateBufferResource(
			dev,
			cmd,
			nullptr,
			m_nLightsCBElementBytes,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			nullptr
		);

		if ( m_pd3dcbLights[i] )
		{
			m_pd3dcbLights[i]->Map(
				0,
				nullptr,
				reinterpret_cast< void** >( &m_pcbMappedLights[i] )
			);
		}
	}

	m_nMaterialsCBElementBytes = ( ( sizeof(MATERIALS) + 255 ) & ~255 );

	for ( UINT i = 0; i < kFrameResourceCount; ++i )
	{
		m_pd3dcbMaterials[i] = ::CreateBufferResource(
			dev,
			cmd,
			nullptr,
			m_nMaterialsCBElementBytes,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			nullptr
		);

		if ( m_pd3dcbMaterials[i] )
		{
			m_pd3dcbMaterials[i]->Map(
				0,
				nullptr,
				reinterpret_cast< void** >( &m_pcbMappedMaterials[i] )
			);
		}
	}

	m_nFrameResourceIndex = 0;

	m_depthFog.CreateConstantBuffer(dev, cmd);
}

void CGameScene::BuildDepthFogResources(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
	m_depthFog.BuildResources(dev, cmd, GetGraphicsRootSignature());
}

void CGameScene::LinkSceneObjects()
{
	GameSceneAttachmentBinder::LinkInput input{};
	input.playerSpotFollower = nullptr;

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

void CGameScene::BuildObjectsCollider()
{
	m_Collision = make_unique<CCollisionSystem>();

	m_Collision->SetHitEffectCallback(
		[ this ](
			CGameObject* weaponObject,
			CGameObject* targetObject)
		{
			if ( !targetObject )
				return;

			auto IsNpcMonsterObject =
				[ ] (CGameObject* obj) -> bool
				{
					if ( !obj )
						return false;

					auto* tag = obj->GetComponent<CActorTagComponent>();
					return tag && tag->kind == EActorKind::NPC;
				};

			auto IsPlayerWeaponObject =
				[ ] (CGameObject* obj) -> bool
				{
					if ( !obj )
						return false;

					auto* collider = obj->GetComponent<CColliderComponent>();
					if ( !collider )
						return false;

					return collider->GetLayer() == kCollisionLayerPlayerWeapon;
				};

			if ( IsNpcMonsterObject(targetObject) && IsPlayerWeaponObject(weaponObject) )
			{
				CGameObject* attacker =
					ResolvePlayerAttackerFromPlayerWeapon(weaponObject);

				if ( attacker )
				{
					ForceMonsterAIChaseTarget(targetObject, attacker);
				}
			}

			XMFLOAT3 hitDir = XMFLOAT3(0.0f, 0.0f, 1.0f);

			if ( weaponObject )
			{
				const XMFLOAT3 weaponPos = weaponObject->GetPosition();
				const XMFLOAT3 targetPos = targetObject->GetPosition();

				XMVECTOR dirV =
					XMVectorSet(
						targetPos.x - weaponPos.x,
						0.0f,
						targetPos.z - weaponPos.z,
						0.0f
					);

				if ( XMVectorGetX(XMVector3LengthSq(dirV)) > 1.0e-6f )
				{
					dirV = XMVector3Normalize(dirV);
					XMStoreFloat3(&hitDir, dirV);
				}
				else
				{
					// weaponObject와 targetObject 위치가 거의 같으면 무기 방향 사용.
					hitDir = GetSafeObjectForward(weaponObject);
				}
			}

			// hitPosition은 아직 정확히 모르므로 nullptr.
			// SpawnBloodSplash 내부에서 targetObject 위치 + y 1m를 사용한다.
			SpawnBloodSplash(targetObject, nullptr, &hitDir);
		}
	);

	for ( auto& obj : m_staticObjects )
	{
		if ( obj )
			m_Collision->RegisterCollider(obj->GetComponent<CColliderComponent>());
	}

	for ( auto& obj : m_skinnedObjects )
	{
		if ( obj )
			m_Collision->RegisterCollider(obj->GetComponent<CColliderComponent>());
	}
}

void CGameScene::ReleaseBuildOnlySceneData()
{
	// ---------------------------------------------------------------------
	// 1) 파일 로딩 원본 데이터
	// ---------------------------------------------------------------------
	ClearVectorAndFreeMemory(m_staticPlacementEntries);

	{
		auto empty = decltype( mSceneCubeBoxColliderTable ){};
		mSceneCubeBoxColliderTable.swap(empty);
	}

#ifndef USING_NETWORK
	ClearVectorAndFreeMemory(m_monsterSpawnEntries);
#endif

	// ---------------------------------------------------------------------
	// 2) build 중간 캐시 / 현재 런타임에서 직접 참조하지 않는 캐시
	// ---------------------------------------------------------------------
	ClearVectorAndFreeMemory(m_staticCollisionMegaGridMasks);

	// BuildStaticRenderObjectCache() 이후에는
	// StaticRenderObjectCache::dynamicWorldMatrix로 복사되어 있음.
	ClearVectorAndFreeMemory(m_staticDynamicWorldMatrixFlags);

	// BuildStaticInstanceGroups() 내부에서만 필요한 objectIndex -> lodEntryIndex 맵.
	ClearVectorAndFreeMemory(m_staticWorldLodEntryIndexByObjectIndex);

	// InstanceGroup 생성 이후 shader 분류 정보는 group에 저장되어 있음.
	ClearUnorderedSetAndFreeMemory(m_treeAlphaClipObjects);
	ClearUnorderedSetAndFreeMemory(m_skinnedAlphaClipObjects);
}