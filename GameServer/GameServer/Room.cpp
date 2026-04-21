#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "Enemy.h"
#include "Building.h"
#include "GameSession.h"
#include "GameArea.h"
#include "ColliderComponent.h"
#include "MonsterAI.h"
#include "Projectile.h"

#include "Protocol.pb.h"
#include "ClientPacketHandler.h"
#include "ReportHelper.h"

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

		if (matched != 9)
			return false;

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
			"../GameServer/MapFIle/MapData_fullstage.txt"
		};

		std::ifstream fin;
		for (const auto& path : candidates)
		{
			fin.open(path);
			if (fin.is_open())
				break;
			fin.clear();
		}

		if (!fin.is_open())
			return false;

		std::string line;
		while (std::getline(fin, line))
		{
			PlacementEntry entry;
			if (!ParsePlacementEntryLine(line, entry))
				continue;
			outEntries.push_back(std::move(entry));
		}

		return !outEntries.empty();
	}

	static GameMath::Vec3 GetInitialPlayerSpawnPosition(uint64 playerId)
	{
		constexpr float kBaseZ = 1.2f;
		constexpr float kSpacingX = 4.0f;
		return GameMath::Vec3(static_cast<float>(playerId) * kSpacingX, 0.0f, kBaseZ);
	}
}

void Room::Enter(PlayerRef player)
{
	player->Build();
	player->SetPosition(GetInitialPlayerSpawnPosition(player->playerId));

	player->SetWeapon(
		static_cast<Protocol::WeaponType>(player->playerId + 1), 0);

	players[player->playerId] = player;
	player->SetActive(false);

	RegisterDynamicCollider(player);
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
	{
		p.second->ownerSession->Send(sendBuffer);
	}
}

bool Room::LoadMonsterSpawnEntries(std::vector<MonsterSpawnEntry>& outEntries)
{
	outEntries.clear();

	const std::vector<std::string> candidates = {
		"MapFIle/monster_spawn_points_little.txt",
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
		if (fin.is_open())
			break;
		fin.clear();
	}

	if (!fin.is_open())
		return false;

	std::string line;
	while (std::getline(fin, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		if (line.rfind("SPAWN|", 0) != 0)
			continue;

		MonsterSpawnEntry entry{};
		char type[32] = {};
		int megaId = -1;
		int megaX = -1;
		int megaZ = -1;
		float px = 0.0f;
		float py = 0.0f;
		float pz = 0.0f;
		float yawDeg = 0.0f;

		const int matched = ::sscanf_s(
			line.c_str(),
			"SPAWN|index=%d|type=\"%31[^\"]\"|mega_id=%d|mega=(%d,%d)|pos=(%f,%f,%f)|yaw_deg=%f",
			&entry.index,
			type, static_cast<unsigned>(_countof(type)),
			&megaId,
			&megaX,
			&megaZ,
			&px,
			&py,
			&pz,
			&yawDeg
		);

		if (matched != 9)
			continue;

		entry.type = type;
		entry.position = GameMath::Vec3(px, py, pz);
		entry.yawDeg = yawDeg;
		outEntries.push_back(std::move(entry));
	}

	std::sort(outEntries.begin(), outEntries.end(), [](const MonsterSpawnEntry& a, const MonsterSpawnEntry& b)
		{
			return a.index < b.index;
		});

	return !outEntries.empty();
}

void Room::BuildRoom()
{
	buildings.clear();
	enemies.clear();
	m_arrowPool.clear();
	m_bulletPool.clear();
	InitializeCollisionSystem();
	InitializeSpatialGrid();

	std::vector<PlacementEntry> entries;

	if (LoadPlacementEntries(entries))
	{
		uint64 buildingId = 1;
		for (size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex)
		{
			const auto& e = entries[entryIndex];
			auto building = std::make_shared<CBuilding>();
			building->SetObjectId(buildingId);
			building->SetPosition(e.position);
			building->SetYaw(GameMath::NormalizeYaw(e.yawDeg));
			building->SetBuildingType(ReportHelper::AssetToBuildingType(e.asset));
			building->SetActive(true);

			RegisterStaticCollider(building);
			RegisterStaticBuildingToGrid(building);

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

	m_navMesh = make_unique<CNavMesh>();
	const std::vector<std::string> navCandidates = {
		"MapFIle/FullStageNavmesh.nvm",
		"MapFIle/Navmesh_FullStage.nvm",
		"GameServer/MapFIle/FullStageNavmesh.nvm",
		"GameServer/MapFIle/Navmesh_FullStage.nvm"
	};
	for (const auto& path : navCandidates)
	{
		if (m_navMesh->LoadFromFile(path))
			break;
	}

	auto SampleEnemySpawn = [&](const GameMath::Vec3& desiredPos)
		{
			if (!m_navMesh)
				return desiredPos;

			GameMath::Vec3 projected{};
			if (m_navMesh->SamplePosition(desiredPos, projected))
				return projected;

			return desiredPos;
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
	{
		cout << "[MonsterSpawn] load failed" << endl;
	}
	else
	{
		cout << "[MonsterSpawn] load success count=" << spawnEntries.size() << endl;
	}

	uint64 nextEnemyId = 0;
	for (const MonsterSpawnEntry& spawn : spawnEntries)
	{
		uint64 enemyId = 0;
		if (spawn.index >= 0 && enemies.find(static_cast<uint64>(spawn.index)) == enemies.end())
			enemyId = static_cast<uint64>(spawn.index);
		else
			enemyId = nextEnemyId;

		while (enemies.find(enemyId) != enemies.end())
			++enemyId;

		nextEnemyId = std::max(nextEnemyId, enemyId + 1);

		const bool isArcher = (spawn.type == "BowMan");
		const Protocol::EnemyType enemyType = isArcher
			? Protocol::ENEMY_TYPE_ARCHER
			: Protocol::ENEMY_TYPE_BASIC;

		auto enemy = make_shared<CEnemy>(enemyId, spawn.type, enemyType, nullptr);
		enemy->Build(SampleEnemySpawn(spawn.position), GameMath::Vec3(0, 0, 0));
		enemy->SetYaw(GameMath::NormalizeYaw(spawn.yawDeg));
		enemy->AddComponent<CMonsterAI>();
		RegisterDynamicCollider(enemy);
		enemies[enemyId] = enemy;
	}

	for (auto& playerPair : players)
		RegisterDynamicCollider(playerPair.second);

	RebuildDynamicGridState();
}

void Room::StartGame(bool ready, uint32 index)
{
	if (players.find(index) == players.end())
		return;

	WRITE_LOCK;
	static Vector<bool> p_ready(4);
	p_ready[index] = ready;

	players[index]->SetActive(ready);

	static Atomic<bool> gameStarted = false;

	if (
		std::all_of(players.begin(), players.end(),
			[&](const auto& player)
			{
				return player.second && player.second->IsActive();
			})
		&&
		players.size() == MaxPlayers)
	{
		if (gameStarted.exchange(true) == false)
		{
			GRoom->DoAsync(&Room::MakeInitStruct, Protocol::S_GAME_START());
		}
	}
	else
	{
		Protocol::S_ENTER_GAME enterGamePkt;
		GRoom->DoAsync(&Room::MakeEnterGameStruct, enterGamePkt);
	}
}

void Room::EndGame()
{
	ShutdownSpatialGrid();
}

void Room::CheckClientReady()
{
	bool allPlayerBuilt = !players.empty();
	for (auto& player : players)
	{
		allPlayerBuilt = allPlayerBuilt && player.second->IsActive();
	}

	if (allPlayerBuilt)
	{
		std::cout << "Game Started!" << endl;
		GRoom->DoTimer(100, &Room::TickAdvance);
	}
	else
	{
		GRoom->DoTimer(100, &Room::CheckClientReady);
	}
}

void Room::SetPlayerReady(bool ready, uint32& playerId)
{
	players[playerId]->SetActive(true);
}

GameAreaRef Room::GetArea(uint32 areaId)
{
	return GameAreaRef();
}

void Room::TransferPlayer(PlayerRef player, uint32 fromAreaId, uint32 toAreaId)
{
	//gameAreas[fromAreaId]->Leave(player);
	//gameAreas[toAreaId]->Enter(player);
}

