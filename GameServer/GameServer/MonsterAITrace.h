#pragma once

#include "MonsterAITypes.h"
#include "Enum.pb.h"

#include <vector>

class CMonsterAI;

enum class EMonsterAITraceEvent : uint32
{
	None = 0,
	TargetAcquired = 1 << 0,
	TargetLost = 1 << 1,
	FirstChaseNotified = 1 << 2,
	AttackStarted = 1 << 3,
	ArrowSpawned = 1 << 4,
	InitialAdvanceCompleted = 1 << 5,
	ReturnStarted = 1 << 6,
	ReturnCompleted = 1 << 7,
	PatrolDirectionChanged = 1 << 8,
	AwakeEntered = 1 << 9,
	SleepEntered = 1 << 10,
	ForcedHomeReset = 1 << 11
};

struct MonsterAITraceEntry
{
	uint32 tick = 0;
	uint64 monsterId = 0;
	uint64 targetId = 0;
	EMonsterAIState previousState = EMonsterAIState::Idle;
	EMonsterAIState state = EMonsterAIState::Idle;
	GameMath::Vec3 position{};
	GameMath::Vec3 lastMoveDir{};
	float yaw = 0.0f;
	Protocol::AnimationType animation = Protocol::ANIMATION_TYPE_IDLE;
	uint32 animationTick = 0;
	uint32 firstChaseNotifyCount = 0;
	uint32 attackStartCount = 0;
	uint32 arrowSpawnCount = 0;
	EMonsterAITraceEvent events = EMonsterAITraceEvent::None;
	bool isAwake = false;
};

class CMonsterAITrace
{
public:
	static void Record(const CMonsterAI& ai, EMonsterAIState previousState,
		EMonsterAITraceEvent events = EMonsterAITraceEvent::None, bool isAwake = true);
	static const std::vector<MonsterAITraceEntry>& GetEntries();
	static void Clear();
};

#if defined(_DEBUG) || defined(MONSTER_AI_TRACE_ENABLED)
class CMonsterAITraceFrame
{
public:
	explicit CMonsterAITraceFrame(const CMonsterAI& ai);
	~CMonsterAITraceFrame();

private:
	const CMonsterAI& m_ai;
	EMonsterAIState m_previousState;
};

#define MONSTER_AI_TRACE_FRAME(ai) CMonsterAITraceFrame monsterAITraceFrame(ai)
#define MONSTER_AI_TRACE(ai, previousState, events, isAwake) \
	CMonsterAITrace::Record((ai), (previousState), (events), (isAwake))
#else
#define MONSTER_AI_TRACE_FRAME(ai) ((void)0)
#define MONSTER_AI_TRACE(ai, previousState, events, isAwake) ((void)0)
#endif
