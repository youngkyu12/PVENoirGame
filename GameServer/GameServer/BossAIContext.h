#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include "GameTypes.h"

class Room;

struct CBossAIContext
{
	Room*  room        = nullptr;
	uint64 bossEnemyId = 0;

	std::unordered_map<std::string, float> cooldowns;
	std::mt19937 rng{ std::random_device{}() };

	float meleeActionElapsed = -1.0f;
	std::unordered_set<uint64> meleeHitPlayerIds;

	float spellActionElapsed    = -1.0f;
	bool  spellProjectileSpawned = false;

	float callRiseElapsed    = -1.0f;
	float callActionElapsed  = -1.0f;
	float callDescendElapsed = -1.0f;
	bool  callSummonDone     = false;
	float callStartY         = 0.0f;
	int   callExecutedCount  = 0;

	bool movedThisUpdate = false;

	bool IsCalling() const
	{
		return callRiseElapsed >= 0.0f || callActionElapsed >= 0.0f || callDescendElapsed >= 0.0f;
	}

	void TickCooldowns(float dt)
	{
		for (auto& [name, remaining] : cooldowns)
			if (remaining > 0.0f) remaining -= dt;

		if (meleeActionElapsed  >= 0.0f) meleeActionElapsed  += dt;
		if (spellActionElapsed  >= 0.0f) spellActionElapsed  += dt;
		if (callRiseElapsed     >= 0.0f) callRiseElapsed     += dt;
		if (callActionElapsed   >= 0.0f) callActionElapsed   += dt;
		if (callDescendElapsed  >= 0.0f) callDescendElapsed  += dt;
	}

	float GetCooldown(const std::string& name) const
	{
		auto it = cooldowns.find(name);
		return (it != cooldowns.end()) ? it->second : 0.0f;
	}

	void SetCooldown(const std::string& name, float sec)
	{
		cooldowns[name] = sec;
	}

	float RandFloat(float min, float max)
	{
		std::uniform_real_distribution<float> dist(min, max);
		return dist(rng);
	}
};
