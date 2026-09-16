#pragma once

#include "MonsterAITypes.h"

class CMonsterAI;

class IMonsterTargetPolicy
{
public:
	virtual ~IMonsterTargetPolicy() = default;
	virtual bool FindTarget(CMonsterAI& ai) const = 0;
};

class IMonsterChasePolicy
{
public:
	virtual ~IMonsterChasePolicy() = default;
	virtual EMonsterMoveResult UpdateChase(CMonsterAI& ai, float dt) const = 0;
};

class IMonsterLifecyclePolicy
{
public:
	virtual ~IMonsterLifecyclePolicy() = default;
	virtual bool UpdateBeforeTargeting(CMonsterAI& ai, float dt) const = 0;
	virtual bool UpdateNoTarget(CMonsterAI& ai, float dt, bool wasChasing) const = 0;
};
