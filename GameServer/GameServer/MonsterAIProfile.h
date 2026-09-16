#pragma once

#include "MonsterAIPolicy.h"

struct MonsterAIProfile
{
	EMonsterAIProfile id = EMonsterAIProfile::FieldIdle;
	const IMonsterTargetPolicy* targetPolicy = nullptr;
	const IMonsterChasePolicy* chasePolicy = nullptr;
	const IMonsterLifecyclePolicy* lifecyclePolicy = nullptr;
};

const MonsterAIProfile& GetMonsterAIProfile(EMonsterAIProfile id);
bool ValidateMonsterAIProfiles();
