#include "pch.h"
#include "MonsterAIProfile.h"
#include "MonsterAIPolicies.h"

namespace
{
	const CRangeConeTargetPolicy g_rangeConeTargetPolicy;
	const CInnerZoneTargetPolicy g_innerZoneTargetPolicy;
	const CBossRoomTargetPolicy g_bossRoomTargetPolicy;
	const CNavMeshHybridChasePolicy g_navMeshHybridChasePolicy;
	const CDirectChasePolicy g_directChasePolicy;
	const CReturnIdleLifecyclePolicy g_returnIdleLifecyclePolicy;
	const CReturnPatrolLifecyclePolicy g_returnPatrolLifecyclePolicy;
	const CSpawnerRushLifecyclePolicy g_spawnerRushLifecyclePolicy;
	const CBossRoomLifecyclePolicy g_bossRoomLifecyclePolicy;
	const CBossRoomExitLifecyclePolicy g_bossRoomExitLifecyclePolicy;

	const MonsterAIProfile g_fieldIdleProfile{
		EMonsterAIProfile::FieldIdle,
		&g_rangeConeTargetPolicy,
		&g_navMeshHybridChasePolicy,
		&g_returnIdleLifecyclePolicy };

	const MonsterAIProfile g_fieldPatrolProfile{
		EMonsterAIProfile::FieldPatrol,
		&g_rangeConeTargetPolicy,
		&g_navMeshHybridChasePolicy,
		&g_returnPatrolLifecyclePolicy };

	const MonsterAIProfile g_spawnerRushProfile{
		EMonsterAIProfile::SpawnerRush,
		&g_innerZoneTargetPolicy,
		&g_directChasePolicy,
		&g_spawnerRushLifecyclePolicy };

	const MonsterAIProfile g_bossRoomPersistentProfile{
		EMonsterAIProfile::BossRoomPersistent,
		&g_bossRoomTargetPolicy,
		&g_directChasePolicy,
		&g_bossRoomLifecyclePolicy };

	const MonsterAIProfile g_bossRoomExitProfile{
		EMonsterAIProfile::BossRoomExit,
		&g_rangeConeTargetPolicy,
		&g_navMeshHybridChasePolicy,
		&g_bossRoomExitLifecyclePolicy };
}

const MonsterAIProfile& GetMonsterAIProfile(EMonsterAIProfile id)
{
	switch (id)
	{
	case EMonsterAIProfile::FieldPatrol:
		return g_fieldPatrolProfile;
	case EMonsterAIProfile::SpawnerRush:
		return g_spawnerRushProfile;
	case EMonsterAIProfile::BossRoomPersistent:
		return g_bossRoomPersistentProfile;
	case EMonsterAIProfile::BossRoomExit:
		return g_bossRoomExitProfile;
	case EMonsterAIProfile::FieldIdle:
	default:
		return g_fieldIdleProfile;
	}
}

bool ValidateMonsterAIProfiles()
{
	constexpr EMonsterAIProfile profileIds[] = {
		EMonsterAIProfile::FieldIdle,
		EMonsterAIProfile::FieldPatrol,
		EMonsterAIProfile::SpawnerRush,
		EMonsterAIProfile::BossRoomPersistent,
		EMonsterAIProfile::BossRoomExit
	};

	for (EMonsterAIProfile id : profileIds)
	{
		const MonsterAIProfile& profile = GetMonsterAIProfile(id);
		if (profile.id != id || !profile.targetPolicy || !profile.chasePolicy || !profile.lifecyclePolicy)
			return false;
	}

	return true;
}
