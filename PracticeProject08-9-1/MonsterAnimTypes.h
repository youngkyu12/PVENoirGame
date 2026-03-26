//------------------------------------------------------- ----------------------
// File: MonsterAnimTypes.h
//-----------------------------------------------------------------------------

#pragma once
#include "stdafx.h"
#include <string>

enum class EMonsterAnimState : uint8_t
{
	Idle = 0,
	Move
};

enum class EMonsterAnimCommand : uint8_t
{
	None = 0,
	Attack,
	Hit,
	Death,
	Appear,
	Call,
	Spell
};

struct MonsterAnimProfile
{
	std::string idleClip;
	std::string moveClip;

	std::string hitClip;
	std::string deathClip;

	std::string attackClip;
	std::string attackNextClip;
	bool attackHasChain = false;

	std::string appearClip;
	std::string callClip;
	std::string spellClip;

	float locomotionBlendTime = 0.15f;
	float actionBlendTime = 0.08f;
	float actionOutBlendTime = 0.12f;
	float chainBlendTime = 0.05f;
};