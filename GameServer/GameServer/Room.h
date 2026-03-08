#pragma once
#include "JobQueue.h"

namespace Protocol
{
    struct S_GAME_START;
    struct S_ENTER_GAME;
}

class Room : public JobQueue
{
public:
    void Enter(PlayerRef player);
    void Leave(PlayerRef player);
    void BroadCastAll(SendBufferRef sendBuffer); // 전체 공지용

public:
    void BuildRoom(); // 방 초기화 (게임 시작 전)
    void StartGame(bool ready, uint32 index);
    void EndGame();

public:
    void TickAdvance();

public:
	void MakeInitStruct(Protocol::S_GAME_START gameStartPkt);
	void MakeEnterGameStruct(Protocol::S_ENTER_GAME enterGamePkt);


public:
    GameAreaRef GetArea(uint32 areaId);
    void TransferPlayer(PlayerRef player, uint32 fromAreaId, uint32 toAreaId);

	map<uint64, EnemyRef> GetEnemies() { return enemies; }

private:
    USE_LOCK;
    map<uint64, PlayerRef> players; // 전체 플레이어 참조
	map<uint64, EnemyRef> fighters; //  특수 적 참조 (옵션)
	map<uint64, EnemyRef> enemies; // 전체 적 참조 (옵션)
    //array<GameAreaRef, 9> gameAreas; // 9개 구역
};

extern shared_ptr<Room> GRoom;

