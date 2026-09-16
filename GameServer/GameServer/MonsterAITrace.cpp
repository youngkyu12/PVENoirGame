#include "pch.h"
#include "MonsterAITrace.h"

#include "MonsterAI.h"
#include "Room.h"
#include "ServerObject.h"

#if defined(_DEBUG) || defined(MONSTER_AI_TRACE_ENABLED)
namespace
{
	struct MonsterAITraceCounts
	{
		uint32 firstChaseNotifyCount = 0;
		uint32 attackStartCount = 0;
		uint32 arrowSpawnCount = 0;
	};

	std::vector<MonsterAITraceEntry> g_entries;
	std::unordered_map<uint64, MonsterAITraceCounts> g_counts;
}
#endif

void CMonsterAITrace::Record(const CMonsterAI& ai, EMonsterAIState previousState,
	EMonsterAITraceEvent events, bool isAwake)
{
#if defined(_DEBUG) || defined(MONSTER_AI_TRACE_ENABLED)
	const CServerObject* owner = ai.GetOwner();
	if (!owner)
		return;

	MonsterAITraceCounts& counts = g_counts[owner->GetObjectId()];
	const uint32 eventBits = static_cast<uint32>(events);
	if ((eventBits & static_cast<uint32>(EMonsterAITraceEvent::FirstChaseNotified)) != 0)
		++counts.firstChaseNotifyCount;
	if ((eventBits & static_cast<uint32>(EMonsterAITraceEvent::AttackStarted)) != 0)
		++counts.attackStartCount;
	if ((eventBits & static_cast<uint32>(EMonsterAITraceEvent::ArrowSpawned)) != 0)
		++counts.arrowSpawnCount;

	MonsterAITraceEntry entry;
	entry.tick = GRoom ? GRoom->GetTick() : 0;
	entry.monsterId = owner->GetObjectId();
	entry.targetId = ai.m_pTarget ? ai.m_pTarget->GetObjectId() : 0;
	entry.previousState = previousState;
	entry.state = ai.m_state;
	entry.position = owner->GetPosition();
	entry.lastMoveDir = owner->GetLastMoveDir();
	entry.yaw = owner->GetYaw();
	entry.animation = owner->GetAnimState();
	entry.animationTick = static_cast<uint32>(owner->GetAnimTick());
	entry.firstChaseNotifyCount = counts.firstChaseNotifyCount;
	entry.attackStartCount = counts.attackStartCount;
	entry.arrowSpawnCount = counts.arrowSpawnCount;
	entry.events = events;
	entry.isAwake = isAwake;
	g_entries.push_back(entry);
#else
	(void)ai;
	(void)previousState;
	(void)events;
	(void)isAwake;
#endif
}

const std::vector<MonsterAITraceEntry>& CMonsterAITrace::GetEntries()
{
#if defined(_DEBUG) || defined(MONSTER_AI_TRACE_ENABLED)
	return g_entries;
#else
	static const std::vector<MonsterAITraceEntry> empty;
	return empty;
#endif
}

void CMonsterAITrace::Clear()
{
#if defined(_DEBUG) || defined(MONSTER_AI_TRACE_ENABLED)
	g_entries.clear();
	g_counts.clear();
#endif
}

#if defined(_DEBUG) || defined(MONSTER_AI_TRACE_ENABLED)
CMonsterAITraceFrame::CMonsterAITraceFrame(const CMonsterAI& ai)
	: m_ai(ai)
	, m_previousState(ai.GetState())
{
}

CMonsterAITraceFrame::~CMonsterAITraceFrame()
{
	CMonsterAITrace::Record(m_ai, m_previousState);
}
#endif
