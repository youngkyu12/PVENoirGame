#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "Enemy.h"
#include "BossEnemy.h"
#include "Building.h"
#include "GameSession.h"
#include "GameArea.h"
#include "ColliderComponent.h"
#include "Projectile.h"
#include "BossScriptHost.h"
#include "BossAIContext.h"

#include "Protocol.pb.h"
#include "ClientPacketHandler.h"
#include "ReportHelper.h"

Room::~Room() = default;

#include <algorithm>
#include <fstream>
#include <cstdio>
#include <unordered_map>

shared_ptr<Room> GRoom = make_shared<Room>();

namespace
{
	struct PlacementEntry
	{
		std::string asset;
		std::string objectName;
		GameMath::Vec3 position = GameMath::Vec3::Zero();
		float yawDeg = 0.0f;
	};

	static float QuaternionToYawDegrees(float x, float y, float z, float w)
	{
		const float siny_cosp = 2.0f * (w * y + x * z);
		const float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
		return atan2f(siny_cosp, cosy_cosp) * GameMath::RAD_TO_DEG;
	}

	static bool ParsePlacementEntryLine(const std::string& line, PlacementEntry& out)
	{
		char asset[64] = {};
		char objectName[128] = {};
		float px = 0.0f, py = 0.0f, pz = 0.0f;
		float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f;

		const int matched = ::sscanf_s(
			line.c_str(),
			"ENTRY|asset=\"%63[^\"]\"|object=\"%127[^\"]\"|pos=(%f,%f,%f)|rot=(%f,%f,%f,%f)",
			asset, static_cast<unsigned>(_countof(asset)),
			objectName, static_cast<unsigned>(_countof(objectName)),
			&px, &py, &pz,
			&qx, &qy, &qz, &qw
		);

		if (matched != 9) return false;

		out.asset = asset;
		out.objectName = objectName;
		out.position = GameMath::Vec3(px, py, pz);
		out.yawDeg = QuaternionToYawDegrees(qx, qy, qz, qw);
		return true;
	}

	static bool LoadPlacementEntries(std::vector<PlacementEntry>& outEntries)
	{
		outEntries.clear();

		const std::vector<std::string> candidates = {
			"MapFIle/MapData_fullstage.txt",
			"GameServer/MapFIle/MapData_fullstage.txt",
			"../GameServer/MapFIle/MapData_fullstage.txt",
			"MapFIle/MapData_fullstage(withBoss).txt",
			"GameServer/MapFIle/MapData_fullstage(withBoss).txt",
			"../GameServer/MapFIle/MapData_fullstage(withBoss).txt"
		};

		std::ifstream fin;
		for (const auto& path : candidates)
		{
			fin.open(path);
			if (fin.is_open())
			{
				cout << "[MapData] load " << path << endl;
				break;
			}
			fin.clear();
		}

		if (!fin.is_open()) return false;

		std::string line;
		while (std::getline(fin, line))
		{
			PlacementEntry entry;
			if (!ParsePlacementEntryLine(line, entry)) continue;
			outEntries.push_back(std::move(entry));
		}
		return !outEntries.empty();
	}

	static bool ShouldAttachPlacementToTerrain(const std::string& asset)
	{
		return asset != "Terrain" &&
			asset != "Water";
	}

	static GameMath::Vec3 GetInitialPlayerSpawnPosition(uint64 playerId)
	{
		constexpr float kBaseZ = -150.0f;
		constexpr float kSpacingX = 2.0f;
		return GameMath::Vec3(static_cast<float>(playerId) * kSpacingX, 0.0f, kBaseZ);
	}

	// ========================================
	// HP / Attack Power Tables
	// ========================================
	constexpr int kHpPlayer   = 100;
	constexpr int kHpGhoul    = 30;
	constexpr int kHpBowMan   = 120;
	constexpr int kHpSwordMan = 120;
	constexpr int kHpMutant   = 240;
	constexpr int kHpBoss     = 4800;

	constexpr int kAtkGhoul  = 5;
	constexpr int kAtkSword  = 10;
	constexpr int kAtkArcher = 10;
	constexpr int kAtkMutant = 20;
	constexpr int kAtkBoss   = 50;

	// ========================================
	// EnemySpawner — Pool 수량 (클라이언트 상수와 동일)
	// ========================================
	constexpr int kSpawnerMega6GhoulCount    = 200;
	constexpr int kSpawnerMega8GhoulCount    = 200;
	constexpr int kSpawnerMega5GhoulCount    = 70;
	constexpr int kSpawnerMega5SwordManCount = 10;
	constexpr int kSpawnerMega5BowManCount   = 10;
	constexpr int kSpawnerMega5MutantCount   = 5;

	// EnemySpawner — 스폰 문 지오메트리 / 웨이브 타이밍
	constexpr int   kSpawnerWallCount        = 4;
	constexpr int   kSpawnerSlotsPerWall     = 5;
	constexpr int   kSpawnerBatchCount       = 10;
	constexpr float kSpawnerBatchIntervalSec = 1.0f;
	constexpr float kSpawnerWallHalfExtent   = 99.0f;
	constexpr float kSpawnerSlotSpacing      = 2.0f;

	// ========================================
	// MonsterAI — Per-type parameters
	// ========================================
	struct MonsterAIParam { float chaseStart; float chaseStop; float attackRange; float moveSpeed; float walkSpeed; };
	constexpr MonsterAIParam kAI_Ghoul    {  10.f,       50.f,   1.5f,  2.f, 1.f };
	constexpr MonsterAIParam kAI_SwordMan {  35.f,       50.f,   3.0f,  8.f, 4.f };
	constexpr MonsterAIParam kAI_BowMan   {  50.f,       50.f,  25.0f,  8.f, 4.f };
	constexpr MonsterAIParam kAI_Mutant   {  25.f,       50.f,   2.7f, 12.f, 5.f };
	constexpr MonsterAIParam kAI_Boss     { 1e6f,        1e6f,   7.0f,  6.f, 6.f };

	const MonsterAIParam* GetMonsterAIParam(const string& typeName)
	{
		if (typeName == "Ghoul")    return &kAI_Ghoul;
		if (typeName == "SwordMan") return &kAI_SwordMan;
		if (typeName == "BowMan")   return &kAI_BowMan;
		if (typeName == "Mutant")   return &kAI_Mutant;
		if (typeName == "Boss")     return &kAI_Boss;
		return nullptr;
	}

	int GetEnemyHp(const string& typeName)
	{
		if (typeName == "Ghoul")    return kHpGhoul;
		if (typeName == "BowMan")   return kHpBowMan;
		if (typeName == "SwordMan") return kHpSwordMan;
		if (typeName == "Mutant")   return kHpMutant;
		if (typeName == "Boss")     return kHpBoss;
		return kHpGhoul;
	}

	int GetEnemyAttackPower(const string& typeName)
	{
		if (typeName == "Ghoul")    return kAtkGhoul;
		if (typeName == "BowMan")   return kAtkArcher;
		if (typeName == "SwordMan") return kAtkSword;
		if (typeName == "Mutant")   return kAtkMutant;
		if (typeName == "Boss")     return kAtkBoss;
		return kAtkGhoul;
	}
}

void Room::Enter(PlayerRef player)
{
	if (static_cast<int>(players.size()) >= MaxPlayers)
	{
		player->ownerSession->Disconnect(L"Room full");
		return;
	}

	player->Build();
	player->SetPosition(SnapToTerrainIfBelow(GetInitialPlayerSpawnPosition(player->playerId)));
	player->SetMaxHp(kHpPlayer);

	// life-state 초기 정상화
	player->OnRespawnEnter(GetAnimClockTick()); // 위치를 내부에서 덮어쓰면 아래 순서 조정 필요
	player->SetPosition(SnapToTerrainIfBelow(GetInitialPlayerSpawnPosition(player->playerId)));

	player->SetWeapon(
		static_cast<Protocol::WeaponType>(player->playerId + 1), 0);

	players[player->playerId] = player;
	player->SetActive(false); // 기존 참여/ready 정책 유지

	RegisterDynamicCollider(player);
	SetObjectCollisionMegaGridMask(player, ComputeObjectCurrentMegaGridMask(player.get()), false);
}

void Room::Leave(PlayerRef player)
{
	if (_collision && player)
	{
		if (auto* collider = player->GetComponent<CColliderComponent>())
			_collision->UnregisterCollider(collider);
	}
	players.erase(player->playerId);
}

void Room::BroadCastAll(SendBufferRef sendBuffer)
{
	for (auto& p : players)
		p.second->ownerSession->Send(sendBuffer);
}

bool Room::HasTerrain() const
{
	return m_serverTerrain && m_serverTerrain->IsLoaded();
}

float Room::GetTerrainGroundHeight(float worldX, float worldZ) const
{
	if (!HasTerrain())
		return 0.0f;

	return m_serverTerrain->SampleHeight(worldX, worldZ);
}

GameMath::Vec3 Room::SnapToTerrainIfBelow(const GameMath::Vec3& pos) const
{
	if (!HasTerrain())
		return pos;

	if (!m_serverTerrain->Contains(pos.x, pos.z))
		return pos;

	const float groundY = m_serverTerrain->SampleHeight(pos.x, pos.z);
	return GameMath::Vec3(pos.x, groundY, pos.z);
}

bool Room::LoadMonsterSpawnEntries(std::vector<MonsterSpawnEntry>& outEntries)
{
	outEntries.clear();

	const std::vector<std::string> candidates = {
		"MapFIle/monster_spawn_points.txt",
		"MapFIle/monster_spawn_points.txt",
		"GameServer/MapFIle/monster_spawn_points_little.txt",
		"GameServer/MapFIle/monster_spawn_points.txt",
		"../GameServer/MapFIle/monster_spawn_points_little.txt",
		"../GameServer/MapFIle/monster_spawn_points.txt"
	};

	std::ifstream fin;
	for (const auto& path : candidates)
	{
		fin.open(path);
		if (fin.is_open()) break;
		fin.clear();
	}
	if (!fin.is_open()) return false;

	std::string line;
	while (std::getline(fin, line))
	{
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.rfind("SPAWN|", 0) != 0) continue;

		MonsterSpawnEntry entry{};
		char type[32] = {};
		int megaId = -1, megaX = -1, megaZ = -1;
		float px = 0.0f, py = 0.0f, pz = 0.0f, yawDeg = 0.0f;

		const int matched = ::sscanf_s(
			line.c_str(),
			"SPAWN|index=%d|type=\"%31[^\"]\"|mega_id=%d|mega=(%d,%d)|pos=(%f,%f,%f)|yaw_deg=%f",
			&entry.index,
			type, static_cast<unsigned>(_countof(type)),
			&megaId, &megaX, &megaZ,
			&px, &py, &pz, &yawDeg
		);

		if (matched != 9) continue;

		entry.megaId = megaId;
		entry.type = type;
		entry.position = GameMath::Vec3(px, py, pz);
		entry.yawDeg = yawDeg;
		outEntries.push_back(std::move(entry));
	}

	std::sort(outEntries.begin(), outEntries.end(),
		[](const MonsterSpawnEntry& a, const MonsterSpawnEntry& b) { return a.index < b.index; });

	return !outEntries.empty();
}

void Room::BuildRoom()
{
	buildings.clear();
	m_elapsedServerMs = static_cast<uint64>(tick.load()) * m_timing.serverTickIntervalMs;
	enemies.clear();
	m_aiAwakeEnemyIds.clear();
	m_castleCenterPlayerIds.clear();
	m_meleeHitKeys.clear();
	m_spawnerKeyMutantIds.clear();
	m_keyPickupUnlockedByMegaGrid.fill(true);
	m_keyPickupUnlockedByMegaGrid[6] = false;
	m_keyPickupUnlockedByMegaGrid[8] = false;
	m_towerDoorPortals.clear();
	m_castleDoorPortals.clear();
	m_arrowPool.clear();
	m_bulletPool.clear();
	m_enemyArrowPool.clear();
	m_bossPoisonPool.clear();
	m_bossPoisonHitMap.clear();
	m_items.clear();
	InitializeCollisionSystem();
	InitializeSpatialGrid();

	m_serverTerrain = make_unique<CServerTerrain>();
	const std::vector<std::string> terrainCandidates = {
		"MapFIle/terrain.raw",
		"GameServer/MapFIle/terrain.raw",
		"../GameServer/MapFIle/terrain.raw"
	};

	bool terrainLoaded = false;
	for (const auto& path : terrainCandidates)
	{
		if (m_serverTerrain->LoadFromFile(path))
		{
			terrainLoaded = true;
			cout << "[Terrain] load success " << path << endl;
			cout << "[Terrain] samples (-600,-200)=" << m_serverTerrain->SampleHeight(-600.0f, -200.0f)
				<< " (0,0)=" << m_serverTerrain->SampleHeight(0.0f, 0.0f)
				<< " (599,999)=" << m_serverTerrain->SampleHeight(599.0f, 999.0f) << endl;
			break;
		}
	}
	if (!terrainLoaded)
		cout << "[Terrain] load skipped" << endl;

	std::vector<PlacementEntry> entries;
	if (LoadPlacementEntries(entries))
	{
		uint64 buildingId = 1;
		for (const auto& e : entries)
		{
			GameMath::Vec3 placementPos = e.position;
			if (ShouldAttachPlacementToTerrain(e.asset) &&
				HasTerrain() &&
				m_serverTerrain->Contains(placementPos.x, placementPos.z))
			{
				placementPos.y = m_serverTerrain->SampleHeight(placementPos.x, placementPos.z) + e.position.y;
			}

			auto building = std::make_shared<CBuilding>();
			building->SetObjectId(buildingId);
			building->SetPosition(placementPos);
			building->SetYaw(GameMath::NormalizeYaw(e.yawDeg));
			building->SetBuildingType(ReportHelper::AssetToBuildingType(e.asset));
			building->SetActive(true);
			RegisterStaticCollider(building);
			RegisterStaticBuildingToGrid(building);
			RegisterDoorPortal(building);
			buildings[buildingId] = building;
			++buildingId;
		}
	}

	MakeFireRateMap();

	for (int i = 0; i < kArrowPoolSize; ++i)
	{
		auto p = ObjectPool<CProjectile>::MakeShared();
		p->SetObjectId(100000 + i);
		p->Deactivate();
		m_arrowPool.push_back(p);
	}

	for (int i = 0; i < kBulletPoolSize; ++i)
	{
		auto p = ObjectPool<CProjectile>::MakeShared();
		p->SetObjectId(200000 + i);
		p->Deactivate();
		m_bulletPool.push_back(p);
	}

	for (int i = 0; i < kEnemyArrowPoolSize; ++i)
	{
		auto p = ObjectPool<CProjectile>::MakeShared();
		p->SetObjectId(300000 + i);
		p->Deactivate();
		m_enemyArrowPool.push_back(p);
	}

	for (int i = 0; i < kBossPoisonPoolSize; ++i)
	{
		auto p = ObjectPool<CProjectile>::MakeShared();
		p->SetObjectId(400000 + i);
		p->Deactivate();
		m_bossPoisonPool.push_back(p);
	}

	m_navMesh = make_unique<CNavMesh>();
	const std::vector<std::string> navCandidates = {
		"MapFIle/Navmesh_FullStage.nvm",
		"GameServer/MapFIle/Navmesh_FullStage.nvm",
		"MapFIle/FullStageNavmeshAll.nvm",
		"GameServer/MapFIle/FullStageNavmeshAll.nvm"
	};
	for (const auto& path : navCandidates)
	{
		if (m_navMesh->LoadFromFile(path)) break;
	}

	auto SampleEnemySpawn = [&](const GameMath::Vec3& desiredPos)
		{
			GameMath::Vec3 pos = desiredPos;
			if (m_navMesh)
			{
				GameMath::Vec3 projected{};
				if (m_navMesh->SamplePosition(pos, projected)) pos = projected;
			}
			return SnapToTerrainIfBelow(pos);
		};

	if (!m_navMesh->IsLoaded())
	{
		m_navMesh.reset();
		cout << "[NavMesh] load failed" << endl;
	}
	else
	{
		cout << "[NavMesh] load success" << endl;
	}

	std::vector<MonsterSpawnEntry> spawnEntries;
	if (!LoadMonsterSpawnEntries(spawnEntries))
		cout << "[MonsterSpawn] load failed" << endl;
	else
		cout << "[MonsterSpawn] load success count=" << spawnEntries.size() << endl;

	// enemyId를 클라이언트 BuildSkinnedBatch 루프 순서와 완전히 일치하도록 부여한다.
	// 각 타입 그룹 내에서 일반 적(spawn file) → 풀 적(dormant) 순으로 묶어서 연속 ID를 부여하면
	// 클라이언트의 npcById[npcIndex] == state.id 가 성립한다.
	// 순서: Ghoul → SwordMan → BowMan → Mutant → Boss
	uint64 nextEnemyId = 0;
	uint64 mega6TriggerMutantId = UINT64_MAX;
	uint64 mega8TriggerMutantId = UINT64_MAX;

	auto applyMonsterAIParams = [](CEnemy* e, const std::string& t)
	{
		auto* ai = e->GetMonsterAI();
		if (!ai) return;
		const MonsterAIParam* p = GetMonsterAIParam(t);
		if (!p) return;
		ai->SetChaseRanges(p->chaseStart, p->chaseStop);
		ai->SetAttackRange(p->attackRange);
		ai->SetMoveSpeed(p->moveSpeed);
		ai->SetWalkMoveSpeed(p->walkSpeed);
		ai->SetPatrolEnabled(t == "SwordMan" || t == "BowMan");

		Protocol::WeaponType wt = Protocol::WEAPON_TYPE_NONE;
		if      (t == "BowMan")   wt = Protocol::WEAPON_TYPE_BOW;
		else if (t == "SwordMan") wt = Protocol::WEAPON_TYPE_SWORD;
		uint32 bullets = 0;
		e->SetWeapon(wt, bullets);
	};

	auto makeSpawnEnemy = [&](const MonsterSpawnEntry& spawn)
	{
		Protocol::EnemyType enemyType = Protocol::ENEMY_TYPE_BASIC;
		if      (spawn.type == "BowMan")   enemyType = Protocol::ENEMY_TYPE_ARCHER;
		else if (spawn.type == "SwordMan") enemyType = Protocol::ENEMY_TYPE_WARRIOR;
		else if (spawn.type == "Mutant")   enemyType = Protocol::ENEMY_TYPE_MUTANT;
		else if (spawn.type == "Boss")     enemyType = Protocol::ENEMY_TYPE_BOSS;

		const uint64 enemyId = nextEnemyId++;
		const bool isBoss = (enemyType == Protocol::ENEMY_TYPE_BOSS);

		shared_ptr<CEnemy> enemy;
		if (isBoss)
			enemy = make_shared<CBossEnemy>(enemyId, spawn.type, enemyType, nullptr);
		else
			enemy = make_shared<CEnemy>(enemyId, spawn.type, enemyType, nullptr);

		enemy->Build(SampleEnemySpawn(spawn.position), GameMath::Vec3(0, 0, 0));
		enemy->SetYaw(GameMath::NormalizeYaw(spawn.yawDeg));
		enemy->SetMaxHp(GetEnemyHp(spawn.type));
		enemy->SetAttackPower(GetEnemyAttackPower(spawn.type));
		enemy->SetActive(!isBoss);
		RegisterDynamicCollider(enemy);
		SetObjectCollisionMegaGridMask(enemy, isBoss ? 0 : ComputeObjectCurrentMegaGridMask(enemy.get()), true);
		enemies[enemyId] = enemy;

		if (isBoss)
		{
			m_bossEnemyId = enemyId;
			m_bossOriginalPos = enemy->GetPosition();
			m_bossOriginalYaw = enemy->GetYaw();
		}

		applyMonsterAIParams(enemy.get(), spawn.type);
		if (auto* ai = enemy->GetMonsterAI())
			ai->SetHomePosition(enemy->GetPosition());

		if (spawn.type == "Mutant")
		{
			if (mega6TriggerMutantId == UINT64_MAX && spawn.megaId == 6)
				mega6TriggerMutantId = enemyId;
			if (mega8TriggerMutantId == UINT64_MAX && spawn.megaId == 8)
				mega8TriggerMutantId = enemyId;
		}
	};

	struct SpawnerPoolSpec { int megaGrid; const char* type; int count; Protocol::EnemyType enemyType; };
	auto makePoolEnemies = [&](const SpawnerPoolSpec& spec)
	{
		const int zeroBased = spec.megaGrid - 1;
		const int mgX = zeroBased % kMegaGridCols;
		const int mgZ = zeroBased / kMegaGridCols;
		const float centerX = kGridMinX + mgX * kMegaGridCellWidth  + kMegaGridCellWidth  * 0.5f;
		const float centerZ = kGridMinZ + mgZ * kMegaGridCellHeight + kMegaGridCellHeight * 0.5f;

		for (int i = 0; i < spec.count; ++i)
		{
			const uint64 enemyId = nextEnemyId++;
			auto dormant = make_shared<CEnemy>(enemyId, spec.type, spec.enemyType, nullptr);
			dormant->Build(GameMath::Vec3(centerX, -100.0f, centerZ), GameMath::Vec3::Zero());
			dormant->SetMaxHp(GetEnemyHp(spec.type));
			dormant->SetAttackPower(GetEnemyAttackPower(spec.type));
			dormant->SetActive(false);
			RegisterDynamicCollider(dormant);
			SetObjectCollisionMegaGridMask(dormant, 0, true);
			enemies[enemyId] = dormant;
			applyMonsterAIParams(dormant.get(), spec.type);
			m_poolEnemyMegaGrid[enemyId] = spec.megaGrid;
		}
	};

	// Ghoul: 일반 → 풀(Mega6, Mega8, Mega5)
	for (const auto& s : spawnEntries) if (s.type == "Ghoul")   makeSpawnEnemy(s);
	makePoolEnemies({ 6, "Ghoul",    kSpawnerMega6GhoulCount,    Protocol::ENEMY_TYPE_BASIC   });
	makePoolEnemies({ 8, "Ghoul",    kSpawnerMega8GhoulCount,    Protocol::ENEMY_TYPE_BASIC   });
	makePoolEnemies({ 5, "Ghoul",    kSpawnerMega5GhoulCount,    Protocol::ENEMY_TYPE_BASIC   });

	// SwordMan: 일반 → 풀
	for (const auto& s : spawnEntries) if (s.type == "SwordMan") makeSpawnEnemy(s);
	makePoolEnemies({ 5, "SwordMan", kSpawnerMega5SwordManCount, Protocol::ENEMY_TYPE_WARRIOR });

	// BowMan: 일반 → 풀
	for (const auto& s : spawnEntries) if (s.type == "BowMan")  makeSpawnEnemy(s);
	makePoolEnemies({ 5, "BowMan",   kSpawnerMega5BowManCount,   Protocol::ENEMY_TYPE_ARCHER  });

	// Mutant: 일반 → 풀
	for (const auto& s : spawnEntries) if (s.type == "Mutant")  makeSpawnEnemy(s);
	makePoolEnemies({ 5, "Mutant",   kSpawnerMega5MutantCount,   Protocol::ENEMY_TYPE_MUTANT  });

	// Boss: 일반만
	for (const auto& s : spawnEntries) if (s.type == "Boss")    makeSpawnEnemy(s);

	if (mega6TriggerMutantId != UINT64_MAX) m_spawnerKeyMutantIds[mega6TriggerMutantId] = 6;
	if (mega8TriggerMutantId != UINT64_MAX) m_spawnerKeyMutantIds[mega8TriggerMutantId] = 8;

	for (auto& playerPair : players)
	{
		RegisterDynamicCollider(playerPair.second);
		SetObjectCollisionMegaGridMask(playerPair.second, ComputeObjectCurrentMegaGridMask(playerPair.second.get()), false);
	}

	RebuildDynamicGridState();
	RebuildMegaGridEnemyIds();
	InitializeItems();
}

void Room::SetPlayerLobbyWeapon(uint32 index, uint32 playerWeapon)
{
	auto playerIt = players.find(index);
	if (playerIt == players.end()) return;

	if (playerWeapon >= Protocol::WEAPON_TYPE_SWORD &&
		playerWeapon <= Protocol::WEAPON_TYPE_CANON)
	{
		playerIt->second->SetWeapon(
			static_cast<Protocol::WeaponType>(playerWeapon), 0);
	}
}

void Room::StartGame(bool ready, uint32 index)
{
	if (players.find(index) == players.end()) return;

	static Vector<bool> p_ready(MaxPlayers);
	p_ready[index] = ready;

	static Atomic<bool> gameStarted = false;

	const bool allReady = std::all_of(p_ready.begin(), p_ready.end(), [](bool b) { return b; });

	if (allReady && players.size() == MaxPlayers)
	{
		if (gameStarted.exchange(true) == false)
			GRoom->DoAsync(&Room::MakeInitStruct, Protocol::S_GAME_START());
	}
	else
	{
		Protocol::S_ENTER_GAME enterGamePkt;
		GRoom->DoAsync(&Room::MakeEnterGameStruct, enterGamePkt);
	}
}

void Room::EndGame()
{
	m_aiAwakeEnemyIds.clear();
	ShutdownSpatialGrid();
}

void Room::CheckClientReady()
{
	const volatile bool allPlayerBuilt = !players.empty() &&
		std::all_of(players.begin(), players.end(),
			[](const auto& kv) { return kv.second && kv.second->IsActive(); });

	if (allPlayerBuilt)
	{
		std::cout << "Game Started!" << endl;
		const uint64 startDelayMs = m_timing.gameStartDelayMs;
		GRoom->DoTimer(startDelayMs, &Room::TickAdvance);
		GRoom->DoTimer(startDelayMs, &Room::FrameStateAdvance);
		GRoom->DoTimer(startDelayMs, &Room::ProcessEnemyAI);
	}
	else
	{
		GRoom->DoTimer(m_timing.clientReadyPollIntervalMs, &Room::CheckClientReady);
	}
}

void Room::SetPlayerReady(bool ready, uint32 playerId)
{
	auto it = players.find(playerId);
	if (it == players.end()) return;
	it->second->SetActive(ready);
}

GameAreaRef Room::GetArea(uint32 areaId)
{
	return GameAreaRef();
}

void Room::TransferPlayer(PlayerRef player, uint32 fromAreaId, uint32 toAreaId)
{
}

// ============================================================
// Phase 3 — 스폰 문 위치 / 방향 계산 (클라이언트 동일 계산식)
// ============================================================

GameMath::Vec3 Room::ComputeSpawnerDoorPosition(int megaGrid, int wall, int slot) const
{
	const int zeroBased = megaGrid - 1;
	const int mgX = zeroBased % kMegaGridCols;
	const int mgZ = zeroBased / kMegaGridCols;

	const float centerX = kGridMinX + mgX * kMegaGridCellWidth  + kMegaGridCellWidth  * 0.5f;
	const float centerZ = kGridMinZ + mgZ * kMegaGridCellHeight + kMegaGridCellHeight * 0.5f;

	wall = std::clamp(wall, 0, kSpawnerWallCount - 1);
	slot = std::clamp(slot, 0, kSpawnerSlotsPerWall - 1);

	const float offset =
		(static_cast<float>(slot) - static_cast<float>(kSpawnerSlotsPerWall - 1) * 0.5f)
		* kSpawnerSlotSpacing;

	switch (wall)
	{
	case 0: return { centerX - kSpawnerWallHalfExtent, 0.0f, centerZ + offset }; // left  → +X
	case 1: return { centerX + kSpawnerWallHalfExtent, 0.0f, centerZ + offset }; // right → -X
	case 2: return { centerX + offset, 0.0f, centerZ - kSpawnerWallHalfExtent }; // bottom→ +Z
	default:return { centerX + offset, 0.0f, centerZ + kSpawnerWallHalfExtent }; // top   → -Z
	}
}

float Room::ComputeSpawnerDoorYaw(int wall) const
{
	switch (wall)
	{
	case 0: return  90.0f;  // left  → center (+X)
	case 1: return -90.0f;  // right → center (-X)
	case 2: return   0.0f;  // bottom→ center (+Z)
	default:return 180.0f;  // top   → center (-Z)
	}
}

// ============================================================
// Phase 4 — dormant enemy 활성화
// ============================================================

CEnemy* Room::ActivateSpawnerEnemy(int megaGrid, Protocol::EnemyType type,
                                   const GameMath::Vec3& pos, float yawDeg)
{
	const GameMath::Vec3 spawnPos = SnapToTerrainIfBelow(pos);
	for (auto& [eid, enemy] : enemies)
	{
		if (enemy->IsActive()) continue;
		if (enemy->type != type) continue;

		auto it = m_poolEnemyMegaGrid.find(eid);
		if (it == m_poolEnemyMegaGrid.end() || it->second != megaGrid) continue;

		enemy->SetPosition(spawnPos);
		enemy->SetYaw(yawDeg);
		enemy->ResetHpToMax();
		enemy->SetActive(true);
		SetObjectCollisionMegaGridMask(enemy, ComputeObjectCurrentMegaGridMask(enemy.get()), true);

		const int zeroBased = megaGrid - 1;
		const int mgX = zeroBased % kMegaGridCols;
		const int mgZ = zeroBased / kMegaGridCols;
		const GameMath::Vec3 megaCenter(
			kGridMinX + mgX * kMegaGridCellWidth  + kMegaGridCellWidth  * 0.5f,
			0.f,
			kGridMinZ + mgZ * kMegaGridCellHeight + kMegaGridCellHeight * 0.5f);
		GameMath::Vec3 toCenter(megaCenter.x - pos.x, 0.f, megaCenter.z - pos.z);
		const float len = toCenter.LengthXZ();
		const GameMath::Vec3 homeDir = (len > 1e-6f)
			? GameMath::Vec3(toCenter.x / len, 0.f, toCenter.z / len)
			: GameMath::Vec3(0.f, 0.f, 1.f);

		if (CMonsterAI* ai = enemy->GetMonsterAI())
		{
			ai->SetHomePosition(spawnPos);
			ai->SetDirectMoveMode(60.f, homeDir, 50.f, megaCenter);
		}

		return enemy.get();
	}
	return nullptr;
}

int Room::ActivateSpawnerDoorBatch(int megaGrid, int batchIndex)
{
	if (batchIndex < 0 || batchIndex >= kSpawnerBatchCount) return 0;

	int activated = 0;
	for (int wall = 0; wall < kSpawnerWallCount; ++wall)
	{
		const float yaw = ComputeSpawnerDoorYaw(wall);
		for (int slot = 0; slot < kSpawnerSlotsPerWall; ++slot)
		{
			const GameMath::Vec3 pos = ComputeSpawnerDoorPosition(megaGrid, wall, slot);
			if (ActivateSpawnerEnemy(megaGrid, Protocol::ENEMY_TYPE_BASIC, pos, yaw))
				++activated;
		}
	}
	return activated;
}

bool Room::BeginSpawnerWave(int megaGrid)
{
	if (megaGrid < 1 || megaGrid > kMegaGridCount) return false;

	SpawnerWaveState& state = m_spawnerWaveStates[static_cast<size_t>(megaGrid)];
	if (state.active) return false;

	state = SpawnerWaveState{};
	state.active = true;

	const int spawned = ActivateSpawnerDoorBatch(megaGrid, state.nextBatchIndex++);
	if (spawned <= 0)
	{
		state = SpawnerWaveState{};
		return false;
	}

	if (state.nextBatchIndex >= kSpawnerBatchCount)
		state.active = false;

	return true;
}

// ============================================================
// Phase 5 — 웨이브 타이머 (TickAdvance 에서 호출)
// ============================================================

void Room::UpdateSpawnerWaves(float dt)
{
	for (int mg = 1; mg <= kMegaGridCount; ++mg)
	{
		SpawnerWaveState& state = m_spawnerWaveStates[static_cast<size_t>(mg)];
		if (!state.active) continue;

		state.accumulatorSec += dt;

		while (state.active && state.accumulatorSec >= kSpawnerBatchIntervalSec)
		{
			state.accumulatorSec -= kSpawnerBatchIntervalSec;

			if (state.nextBatchIndex >= kSpawnerBatchCount)
			{
				state.active = false;
				break;
			}

			ActivateSpawnerDoorBatch(mg, state.nextBatchIndex++);

			if (state.nextBatchIndex >= kSpawnerBatchCount)
				state.active = false;
		}
	}
}

// ============================================================
// Phase 6 — 트리거 연결
// ============================================================

void Room::OnMonsterFirstChase(uint64 enemyId)
{
	auto it = m_spawnerKeyMutantIds.find(enemyId);
	if (it == m_spawnerKeyMutantIds.end()) return;

	const int megaGrid = it->second;

	if (megaGrid == 6 && IsMegaGridCleared(3)) return;
	if (megaGrid == 8 && IsMegaGridCleared(7)) return;

	MegaGridCell& cell = m_megaGridCells[static_cast<size_t>(megaGrid - 1)];
	if (cell.hasEventOccurred) return;

	if (BeginSpawnerWave(megaGrid))
	{
		cell.hasEventOccurred = true;
		cout << "[Spawner] MegaGrid " << megaGrid << " wave triggered by enemy " << enemyId << endl;
	}
}

void Room::OnMonsterDeath(uint64 enemyId)
{
	auto it = m_spawnerKeyMutantIds.find(enemyId);
	if (it != m_spawnerKeyMutantIds.end())
	{
		const int megaGrid = it->second;
		if (megaGrid == 6 || megaGrid == 8)
		{
			m_keyPickupUnlockedByMegaGrid[static_cast<size_t>(megaGrid)] = true;
			cout << "[Key Unlock] MegaGrid " << megaGrid
				<< " unlocked by enemy " << enemyId << " death" << endl;
		}
		m_spawnerKeyMutantIds.erase(it);
	}

	if (m_bossRoomState == EBossRoomState::PreBossCombat && AreAllPreBossMonstersDeadInMega5())
	{
		m_bossRoomState = EBossRoomState::SummonFadeIn;
		m_bossRoomStateChangedMs = m_elapsedServerMs;
		cout << "[BossRoom] All pre-boss monsters dead -> SummonFadeIn" << endl;
	}

	if (enemyId == m_bossEnemyId && m_bossRoomState == EBossRoomState::BossActive)
	{
		m_bossRoomState = EBossRoomState::BossDead;
		m_bossRoomStateChangedMs = m_elapsedServerMs;
		cout << "[BossRoom] BossDead" << endl;
	}
}

void Room::DebugKillMega5Enemies()
{
	const uint32 animTick = GetAnimClockTick();
	for (auto& [id, enemy] : enemies)
	{
		if (!enemy || enemy->IsDead() || !enemy->IsActive()) continue;
		if (enemy->type == Protocol::ENEMY_TYPE_BOSS) continue;
		if (!IsPositionInsideMegaGridNumber(enemy->GetPosition(), 5)) continue;
		enemy->ApplyHit(animTick, 999999, 0);
	}
}

void Room::DebugTeleportToMegaGrid(uint64 playerId, int megaGridNumber)
{
	auto it = players.find(playerId);
	if (it == players.end() || !it->second) return;

	float minX, maxX, minZ, maxZ;
	if (!GetMegaGridCenterMovementBounds(megaGridNumber, minX, maxX, minZ, maxZ)) return;

	float cx = (minX + maxX) * 0.5f;
	float cz = (minZ + maxZ) * 0.5f;
	auto pos = it->second->GetPosition();
	it->second->SetPosition(cx, pos.y, cz);
	cout << "[Debug] Player " << playerId << " teleported to MegaGrid " << megaGridNumber
		<< " (" << cx << ", " << cz << ")" << endl;
}
