#pragma once

#include "GameMath.h"

class CServerObject;

enum class EMonsterAIProfile : uint8
{
	FieldIdle,
	FieldPatrol,
	SpawnerRush,
	BossRoomPersistent,
	BossRoomExit
};

enum class EMonsterAIState : uint8
{
	InitialAdvance,
	Idle,
	Patrol,
	Chase,
	Attack,
	ReturnHome
};

enum class EMonsterTargetResult : uint8
{
	None,
	Acquired,
	Retained,
	Lost
};

enum class EMonsterMoveResult : uint8
{
	NotHandled,
	Moving,
	Reached,
	Blocked,
	PathNotFound
};

struct MonsterAIProfileTransition
{
	float initialAdvanceDistance = 0.0f;
	GameMath::Vec3 initialAdvanceDirection{};
	float innerZoneRadius = 0.0f;
	GameMath::Vec3 innerZoneCenter{};
	bool beginReturnHome = false;

	static MonsterAIProfileTransition EnterSpawnerRush(
		float advanceDistance,
		const GameMath::Vec3& advanceDirection,
		float zoneRadius,
		const GameMath::Vec3& zoneCenter)
	{
		MonsterAIProfileTransition transition;
		transition.initialAdvanceDistance = advanceDistance;
		transition.initialAdvanceDirection = advanceDirection;
		transition.innerZoneRadius = zoneRadius;
		transition.innerZoneCenter = zoneCenter;
		return transition;
	}

	static MonsterAIProfileTransition EnterBossRoom()
	{
		return {};
	}

	static MonsterAIProfileTransition LeaveBossRoom()
	{
		MonsterAIProfileTransition transition;
		transition.beginReturnHome = true;
		return transition;
	}
};
