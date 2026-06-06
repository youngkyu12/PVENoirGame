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

	void TickCooldowns(float dt)
	{
		for (auto& [name, remaining] : cooldowns)
			if (remaining > 0.0f) remaining -= dt;

		if (meleeActionElapsed >= 0.0f) meleeActionElapsed += dt;
		if (spellActionElapsed  >= 0.0f) spellActionElapsed  += dt;
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
