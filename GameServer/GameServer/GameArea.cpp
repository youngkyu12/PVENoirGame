#include "pch.h"
#include "GameArea.h"
#include "Player.h"
#include "Enemy.h"
#include "Building.h"
#include "GameSession.h"

#include "MapData.h"

void GameArea::CreateObjects()
{
    auto mapCfg = MapData::GetDefaultMap();

    // 플레이어 스폰 포인트 확인
    auto playerSpawns = mapCfg.GetSpawnsByType(MapData::EntityType::Player);
    printf("플레이어 스폰 포인트: %zu개\n", playerSpawns.size());

    // 적 초기화
    for (const auto& spawn : mapCfg.spawns)
    {
        if (spawn.type == MapData::EntityType::Player)
            continue;

        // 서버 객체 생성
		auto enemy = make_shared<CEnemy>();
        enemy->SetPosition(spawn.posX, spawn.posY, spawn.posZ);
        enemy->SetYaw(spawn.yaw);

        // 타일 위치 계산
        int tileX = mapCfg.tiles.WorldToTileX(spawn.posX);
        int tileZ = mapCfg.tiles.WorldToTileZ(spawn.posZ);
        printf("[%s] ID=%u 위치=(%.1f, %.1f) 타일=(%d, %d)\n",
            MapData::EntityTypeToString(spawn.type),
            spawn.id, spawn.posX, spawn.posZ, tileX, tileZ);
    }
}


void GameArea::SpawnEnemy(EnemyRef enemy)
{
}

void GameArea::RemoveEnemy(uint64 enemyId)
{
}

void GameArea::Enter(PlayerRef player)
{
	_players[player->playerId] = player;
}

void GameArea::Leave(PlayerRef player)
{
	_players.erase(player->playerId);
}

void GameArea::BroadCast(SendBufferRef sendBuffer)
{
	for (auto& p : _players)
	{
		p.second->ownerSession->Send(sendBuffer);
	}
}