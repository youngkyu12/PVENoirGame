#pragma once

#include <chrono>

enum class EMonsterAIPerformanceCounter
{
	AcquireTarget,
	NavMeshLineOfSight,
	FindPath,
	DirectMove,
	FollowPath
};

class CMonsterAIPerformance
{
public:
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

	static void BeginBatch();
	static void RecordMonsterUpdate(TimePoint startedAt);
	static void Increment(EMonsterAIPerformanceCounter counter);
	static void EndBatch();
};

#if defined(MONSTER_AI_PROFILE_ENABLED)
#define MONSTER_AI_PERF_BEGIN_BATCH() CMonsterAIPerformance::BeginBatch()
#define MONSTER_AI_PERF_MONSTER_START(name) const auto name = CMonsterAIPerformance::Clock::now()
#define MONSTER_AI_PERF_MONSTER_END(name) CMonsterAIPerformance::RecordMonsterUpdate(name)
#define MONSTER_AI_PERF_INCREMENT(counter) CMonsterAIPerformance::Increment(counter)
#define MONSTER_AI_PERF_END_BATCH() CMonsterAIPerformance::EndBatch()
#else
#define MONSTER_AI_PERF_BEGIN_BATCH() ((void)0)
#define MONSTER_AI_PERF_MONSTER_START(name) ((void)0)
#define MONSTER_AI_PERF_MONSTER_END(name) ((void)0)
#define MONSTER_AI_PERF_INCREMENT(counter) ((void)0)
#define MONSTER_AI_PERF_END_BATCH() ((void)0)
#endif
