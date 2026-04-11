#pragma once
#include "JobQueue.h"
#include "CollisionSystem.h"
#include "NavMesh.h"

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
    void ProcessInput(uint64 playerId, int32 keyCodes, float deltaX, float deltaY);

public:
    void MakeFrameState(uint32 tick);
	void MakeInitStruct(Protocol::S_GAME_START gameStartPkt);
	void MakeEnterGameStruct(Protocol::S_ENTER_GAME enterGamePkt);

	// TODO: 모든 플레이어가 ready를 보냈는지 확인하는 함수
    void CheckClientReady();

public:
    void SetPlayerReady(bool ready, uint32& playerId);

public:
    GameAreaRef GetArea(uint32 areaId);
    void TransferPlayer(PlayerRef player, uint32 fromAreaId, uint32 toAreaId);

	map<uint64, EnemyRef> GetEnemies() { return enemies; }
	const map<uint64, PlayerRef>& GetPlayers() const { return players; }
	const CNavMesh* GetNavMesh() const { return m_navMesh.get(); }

private:
	void InitializeCollisionSystem();
    void RegisterStaticCollider(BuildingRef building);
	void RegisterDynamicCollider(const shared_ptr<CServerObject>& obj);

	CCollisionSystem* GetCollisionSystem() const { return _collision.get(); }

	void ResolveWorldStaticCollision(const shared_ptr<CServerObject>& obj, const GameMath::Vec3& previousPos);
	GameMath::Vec3 ResolvePreBlockedShift(const shared_ptr<CServerObject>& obj, const GameMath::Vec3& desiredShift);

	void FireArrow(PlayerRef shooter);
	void FireCannonball(PlayerRef shooter);
	ProjectileRef AcquireFromPool(Vector<ProjectileRef>& pool);

	std::unique_ptr<CNavMesh> m_navMesh;
	static constexpr int kArrowPoolSize = 64;
	static constexpr int kBulletPoolSize = 64;
	Vector<ProjectileRef> m_arrowPool;
	Vector<ProjectileRef> m_bulletPool;

	std::unique_ptr<CCollisionSystem> _collision;

    USE_LOCK;
    map<uint64, PlayerRef> players; // 전체 플레이어 참조
	map<uint64, EnemyRef> fighters; //  특수 적 참조 (옵션)
	map<uint64, EnemyRef> enemies; // 전체 적 참조 (옵션)
	map<uint64, BuildingRef> buildings; // 맵 파일 기반 정적 오브젝트
    //array<GameAreaRef, 9> gameAreas; // 9개 구역

    Atomic<uint32> tick = 0;
};

extern shared_ptr<Room> GRoom;
constexpr int MaxPlayers = 2;

