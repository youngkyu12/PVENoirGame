#pragma once

#include "MonsterAIPolicy.h"

class CRangeConeTargetPolicy final : public IMonsterTargetPolicy
{
public:
	bool FindTarget(CMonsterAI& ai) const override;
};

class CInnerZoneTargetPolicy final : public IMonsterTargetPolicy
{
public:
	bool FindTarget(CMonsterAI& ai) const override;
};

class CBossRoomTargetPolicy final : public IMonsterTargetPolicy
{
public:
	bool FindTarget(CMonsterAI& ai) const override;
};

class CNavMeshHybridChasePolicy final : public IMonsterChasePolicy
{
public:
	EMonsterMoveResult UpdateChase(CMonsterAI& ai, float dt) const override;
};

class CDirectChasePolicy final : public IMonsterChasePolicy
{
public:
	EMonsterMoveResult UpdateChase(CMonsterAI& ai, float dt) const override;
};

class CReturnIdleLifecyclePolicy final : public IMonsterLifecyclePolicy
{
public:
	bool UpdateBeforeTargeting(CMonsterAI& ai, float dt) const override;
	bool UpdateNoTarget(CMonsterAI& ai, float dt, bool wasChasing) const override;
};

class CReturnPatrolLifecyclePolicy final : public IMonsterLifecyclePolicy
{
public:
	bool UpdateBeforeTargeting(CMonsterAI& ai, float dt) const override;
	bool UpdateNoTarget(CMonsterAI& ai, float dt, bool wasChasing) const override;
};

class CSpawnerRushLifecyclePolicy final : public IMonsterLifecyclePolicy
{
public:
	bool UpdateBeforeTargeting(CMonsterAI& ai, float dt) const override;
	bool UpdateNoTarget(CMonsterAI& ai, float dt, bool wasChasing) const override;
};

class CBossRoomLifecyclePolicy final : public IMonsterLifecyclePolicy
{
public:
	bool UpdateBeforeTargeting(CMonsterAI& ai, float dt) const override;
	bool UpdateNoTarget(CMonsterAI& ai, float dt, bool wasChasing) const override;
};

class CBossRoomExitLifecyclePolicy final : public IMonsterLifecyclePolicy
{
public:
	bool UpdateBeforeTargeting(CMonsterAI& ai, float dt) const override;
	bool UpdateNoTarget(CMonsterAI& ai, float dt, bool wasChasing) const override;
};
