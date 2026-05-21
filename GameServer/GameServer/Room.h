#pragma once
#include "JobQueue.h"
#include "CollisionSystem.h"
#include "NavMesh.h"
#include <array>
#include <vector>
#include <unordered_set>

namespace Protocol
{
    struct S_GAME_START;
    struct S_ENTER_GAME;
}

struct RoomTimingConfig
{
	uint64 serverTickIntervalMs = 16;
	uint64 frameStateIntervalMs = 100;
	uint64 enemyAiIntervalMs = 250;
	uint64 clientReadyPollIntervalMs = 100;
	uint64 gameStartDelayMs = 100;

	float playerInputDtSec = 0.016f;
	float enemyAiDtSec = 0.3f;
	float enemyAiWakeRange = 100.0f;
	float enemyAiSleepRange = 120.0f;

	float projectileDtSec = 0.016f;
	uint64 projectileLifeTickMs = 16;

	uint64 animClockIntervalMs = 60;
	uint64 combatClockIntervalMs = 60;
};

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
    void ProcessInput(uint64 playerId, int32 keyCodes, float deltaX, float deltaY, float clientDeltaTime);

public:
    void MakeFrameState(uint32 tick);
	void FrameStateAdvance();
	void MakeInitStruct(Protocol::S_GAME_START gameStartPkt);
	void MakeEnterGameStruct(Protocol::S_ENTER_GAME enterGamePkt);

	// TODO: 모든 플레이어가 ready를 보냈는지 확인하는 함수
    void CheckClientReady();

public:
	// Enemy에 대한 AI 처리 함수를 워커 쓰레드가 꺼내 쓸 수 있도록 따로 함수를 판다
	void ProcessEnemyAI();

public:
    void SetPlayerReady(bool ready, uint32 playerId);

public:
    GameAreaRef GetArea(uint32 areaId);
    void TransferPlayer(PlayerRef player, uint32 fromAreaId, uint32 toAreaId);

	map<uint64, EnemyRef> GetEnemies() { return enemies; }
	const map<uint64, PlayerRef>& GetPlayers() const { return players; }
	const CNavMesh* GetNavMesh() const { return m_navMesh.get(); }
	uint32 GetTick() const { return tick.load(); }
	uint64 GetElapsedServerMs() const { return m_elapsedServerMs; }
	const RoomTimingConfig& GetTimingConfig() const { return m_timing; }
	uint32 GetAnimClockTick() const
	{
		if (m_timing.animClockIntervalMs == 0)
			return tick.load();
		return static_cast<uint32>(m_elapsedServerMs / m_timing.animClockIntervalMs);
	}
	uint32 GetCombatClockTick() const
	{
		if (m_timing.combatClockIntervalMs == 0)
			return tick.load();
		return static_cast<uint32>(m_elapsedServerMs / m_timing.combatClockIntervalMs);
	}

private:
	void InitializeCollisionSystem();
    void RegisterStaticCollider(BuildingRef building);
	void RegisterDynamicCollider(const shared_ptr<CServerObject>& obj);

	CCollisionSystem* GetCollisionSystem() const { return _collision.get(); }

	void ResolveWorldStaticCollision(const shared_ptr<CServerObject>& obj, const GameMath::Vec3& previousPos);
	GameMath::Vec3 ResolvePreBlockedShift(const shared_ptr<CServerObject>& obj, const GameMath::Vec3& desiredShift);

	void FireArrow(PlayerRef shooter, float speed, uint32 lifeTicks);
	void FireCannonball(PlayerRef shooter);
	ProjectileRef AcquireFromPool(Vector<ProjectileRef>& pool);
	void WakeEnemiesNearPlayer(const PlayerRef& player);
	bool IsEnemyNearAnyPlayerExact(const GameMath::Vec3& enemyPos, float rangeSq) const;

	void InitializeSpatialGrid();
	void ShutdownSpatialGrid();
	void InitializeMegaGridState();

	bool WorldToGridCell(float worldX, float worldZ, int& outCellX, int& outCellZ) const;
	int GridCellIndex(int cellX, int cellZ) const;
	bool GetGridCellRangeForWorldBounds(
		float minWorldX,
		float maxWorldX,
		float minWorldZ,
		float maxWorldZ,
		int& outMinCellX,
		int& outMaxCellX,
		int& outMinCellZ,
		int& outMaxCellZ) const;

	int MegaGridIndex(int megaX, int megaZ) const;
	bool WorldToMegaGridCell(float worldX, float worldZ, int& outMegaX, int& outMegaZ) const;
	bool GetMegaGridRangeForCircle(
		const GameMath::Vec3& center,
		float radius,
		int& outMinMegaX,
		int& outMaxMegaX,
		int& outMinMegaZ,
		int& outMaxMegaZ) const;
	void CollectEnemyIdsInMegaGridRadius(
		const GameMath::Vec3& center,
		float radius,
		std::vector<uint64>& outEnemyIds) const;
	bool FineCellToMegaGridCell(int cellX, int cellZ, int& outMegaX, int& outMegaZ) const;
	bool IsFineCellInsideMegaGridApproachZone(int megaX, int megaZ, int cellX, int cellZ) const;
	uint16_t ComputeMegaGridMaskFromWorldPosition(const GameMath::Vec3& pos) const;
	uint16_t ComputeObjectCurrentMegaGridMask(const CServerObject* obj) const;
	uint16_t ComputeStaticBuildingMegaGridMask(const BuildingRef& building) const;
	void SetObjectCollisionMegaGridMask(const shared_ptr<CServerObject>& obj, uint16_t mask, bool fixedMask);
	void RefreshDynamicCollisionMegaGridMasks();
	bool ShouldKeepCollisionPairByMegaGrid(const CColliderComponent* a, const CColliderComponent* b) const;

	enum class EGridDynamicKind : uint8_t
	{
		Player,
		Monster,
		Arrow,
		Bullet
	};

	struct GridStaticCell
	{
		uint16_t buildingCount = 0;
		float floorHeight = 0.0f;
		std::vector<uint64> buildingIds;
	};

	struct GridDynamicCell
	{
		uint16_t playerCount = 0;
		uint16_t monsterCount = 0;
		uint16_t arrowCount = 0;
		uint16_t bulletCount = 0;
	};

	struct MegaGridCell
	{
		bool hasPlayerApproached = false;
		bool isCleared = false;
		bool hasEventOccurred = false;

		int approachWidthCells = 200;
		int approachHeightCells = 200;

		std::vector<uint64> enemyIds;
	};

	struct GridDynamicTracker
	{
		CServerObject* object = nullptr;
		int prevCellX = -1;
		int prevCellZ = -1;
		bool occupied = false;
	};

	struct DoorPortalSubBoxRef
	{
		size_t subIndex = static_cast<size_t>(-1);
	};

	struct TowerDoorPortalEntry
	{
		BuildingRef tower;
		CColliderComponent* collider = nullptr;
		std::vector<DoorPortalSubBoxRef> doorARefs;
		std::vector<DoorPortalSubBoxRef> doorBRefs;
		int cooldownTicks = 0;
	};

	struct CastleDoorPortalPair
	{
		int sourceDoorIndex = -1;
		int targetDoorIndex = -1;
		std::vector<DoorPortalSubBoxRef> sourceRefs;
		std::vector<DoorPortalSubBoxRef> targetRefs;
	};

	struct CastleDoorPortalEntry
	{
		BuildingRef castle;
		CColliderComponent* collider = nullptr;
		std::array<std::vector<DoorPortalSubBoxRef>, 8> doorRefsByIndex;
		std::vector<CastleDoorPortalPair> pairs;
		int cooldownTicks = 0;
	};

	void AddDynamicCount(int cellX, int cellZ, EGridDynamicKind kind, int delta);
	void RegisterStaticBuildingToGrid(BuildingRef building);
	void CollectStaticBuildingIdsForWorldBounds(
		float minWorldX,
		float maxWorldX,
		float minWorldZ,
		float maxWorldZ,
		std::vector<uint64>& outBuildingIds) const;
	bool HasCollisionWithNearbyWorldStatic(const CColliderComponent* subject) const;
	void RebuildMegaGridEnemyIds();
	void RegisterDoorPortal(BuildingRef building);
	void TickDoorPortalCooldowns();
	void ProcessDoorPortals();
	bool TryTeleportPlayerByTowerDoorPortal(const PlayerRef& player);
	bool TryTeleportPlayerByCastleDoorPortal(const PlayerRef& player);
	bool CanUseCastleDoorPortal() const;
	int CountClearedMegaGrids() const;
	void MarkPlayerEnteredCastleCenterMegaGrid(uint64 playerId);
	bool IsPlayerInsideCastleCenterMegaGridFullArea(uint64 playerId) const;
	void UpdateCastleCenterMegaGridState();

	void ResetDynamicGridCounts();
	bool TryGetTrackedCell(const CServerObject* obj, int& outCellX, int& outCellZ) const;
	void RefreshDynamicTracker(GridDynamicTracker& tracker, EGridDynamicKind kind);
	void BuildDynamicGridTrackers();
	void RebuildDynamicGridState();
	void UpdateDynamicGridState();
	void UpdateMegaGridState();

	struct MonsterSpawnEntry
	{
		int index = -1;
		std::string type;
		GameMath::Vec3 position = GameMath::Vec3::Zero();
		float yawDeg = 0.0f;
	};

	bool LoadMonsterSpawnEntries(std::vector<MonsterSpawnEntry>& outEntries);

	static constexpr int kGridMinX = -600;
	static constexpr int kGridMaxX = 600;
	static constexpr int kGridMinZ = -200;
	static constexpr int kGridMaxZ = 1000;

	static constexpr int kGridWidth = (kGridMaxX - kGridMinX);
	static constexpr int kGridHeight = (kGridMaxZ - kGridMinZ);
	static constexpr int kGridCellCount = (kGridWidth * kGridHeight);

	static constexpr int kMegaGridCols = 3;
	static constexpr int kMegaGridRows = 3;
	static constexpr int kMegaGridCount = (kMegaGridCols * kMegaGridRows);

	static constexpr int kMegaGridCellWidth = (kGridWidth / kMegaGridCols);
	static constexpr int kMegaGridCellHeight = (kGridHeight / kMegaGridRows);

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

	bool m_spatialGridInitialized = false;
	std::vector<GridStaticCell> m_gridStaticCells;
	std::vector<GridDynamicCell> m_gridDynamicCells;
	std::array<MegaGridCell, kMegaGridCount> m_megaGridCells = {};

	std::vector<GridDynamicTracker> m_playerGridTrackers;
	std::vector<GridDynamicTracker> m_monsterGridTrackers;
	std::vector<GridDynamicTracker> m_arrowGridTrackers;
	std::vector<GridDynamicTracker> m_bulletGridTrackers;
	std::unordered_set<uint64> m_aiAwakeEnemyIds;
	std::unordered_set<uint64> m_castleCenterPlayerIds;
	std::vector<TowerDoorPortalEntry> m_towerDoorPortals;
	std::vector<CastleDoorPortalEntry> m_castleDoorPortals;
    //array<GameAreaRef, 9> gameAreas; // 9개 구역

	RoomTimingConfig m_timing;
	uint64 m_elapsedServerMs = 0;

    Atomic<uint32> tick = 0;
};

extern shared_ptr<Room> GRoom;
constexpr int MaxPlayers = 1;

