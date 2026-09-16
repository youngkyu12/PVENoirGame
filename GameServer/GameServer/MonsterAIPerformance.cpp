#include "pch.h"
#include "MonsterAIPerformance.h"

#include <numeric>

#if defined(MONSTER_AI_PROFILE_ENABLED)
namespace
{
	struct MonsterAIPerformanceBatch
	{
		std::vector<uint64> monsterNanoseconds;
		uint64 acquireTargetCount = 0;
		uint64 lineOfSightCount = 0;
		uint64 findPathCount = 0;
		uint64 directMoveCount = 0;
		uint64 followPathCount = 0;
	};

	MonsterAIPerformanceBatch g_batch;
}
#endif

void CMonsterAIPerformance::BeginBatch()
{
#if defined(MONSTER_AI_PROFILE_ENABLED)
	g_batch = {};
#endif
}

void CMonsterAIPerformance::RecordMonsterUpdate(TimePoint startedAt)
{
#if defined(MONSTER_AI_PROFILE_ENABLED)
	const uint64 elapsed = static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - startedAt).count());
	g_batch.monsterNanoseconds.push_back(elapsed);
#else
	(void)startedAt;
#endif
}

void CMonsterAIPerformance::Increment(EMonsterAIPerformanceCounter counter)
{
#if defined(MONSTER_AI_PROFILE_ENABLED)
	switch (counter)
	{
	case EMonsterAIPerformanceCounter::AcquireTarget:       ++g_batch.acquireTargetCount; break;
	case EMonsterAIPerformanceCounter::NavMeshLineOfSight: ++g_batch.lineOfSightCount; break;
	case EMonsterAIPerformanceCounter::FindPath:            ++g_batch.findPathCount; break;
	case EMonsterAIPerformanceCounter::DirectMove:          ++g_batch.directMoveCount; break;
	case EMonsterAIPerformanceCounter::FollowPath:          ++g_batch.followPathCount; break;
	}
#else
	(void)counter;
#endif
}

void CMonsterAIPerformance::EndBatch()
{
#if defined(MONSTER_AI_PROFILE_ENABLED)
	if (g_batch.monsterNanoseconds.empty())
		return;

	std::sort(g_batch.monsterNanoseconds.begin(), g_batch.monsterNanoseconds.end());
	const size_t count = g_batch.monsterNanoseconds.size();
	const uint64 total = std::accumulate(
		g_batch.monsterNanoseconds.begin(), g_batch.monsterNanoseconds.end(), uint64{ 0 });
	const uint64 p50 = g_batch.monsterNanoseconds[(count - 1) * 50 / 100];
	const uint64 p95 = g_batch.monsterNanoseconds[(count - 1) * 95 / 100];
	const uint64 maximum = g_batch.monsterNanoseconds.back();

	cout << "[MonsterAI.Perf] count=" << count
		<< " total_us=" << total / 1000
		<< " avg_ns=" << total / count
		<< " p50_ns=" << p50
		<< " p95_ns=" << p95
		<< " max_ns=" << maximum
		<< " acquire=" << g_batch.acquireTargetCount
		<< " los=" << g_batch.lineOfSightCount
		<< " astar=" << g_batch.findPathCount
		<< " direct=" << g_batch.directMoveCount
		<< " path=" << g_batch.followPathCount
		<< endl;
#endif
}
