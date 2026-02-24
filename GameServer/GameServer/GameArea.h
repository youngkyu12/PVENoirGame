#pragma once
#include "JobQueue.h"

class GameArea : public JobQueue
{
public:
	GameArea() = default;
	~GameArea() = default;

public:
	// 게임 영역에 적 추가
	void AddEnemy(EnemyRef enemy) 
    { 
        _enemies.insert({ _enemies.size() - 1, enemy });
    }

	const map<uint64, EnemyRef>& GetEnemies() const { return _enemies; }
	// 게임 영역에 건물 추가
	void AddBuilding(BuildingRef building) { _buildings.push_back(building); }
	const vector<BuildingRef>& GetBuildings() const { return _buildings; }

	// 게임 영역 초기화 (적, 건물 등 생성)
	void CreateObjects();
    void Enter(PlayerRef player);
    void Leave(PlayerRef player);
    void BroadCast(SendBufferRef sendBuffer);

    void SpawnEnemy(EnemyRef enemy);
    void RemoveEnemy(uint64 enemyId);

    //void AddItem(ItemRef item);

private:
    uint32 _areaId;
    map<uint64, PlayerRef> _players;
    map<uint64, EnemyRef> _enemies;
    vector<BuildingRef> _buildings;
    //vector<ItemRef> _items;
};
