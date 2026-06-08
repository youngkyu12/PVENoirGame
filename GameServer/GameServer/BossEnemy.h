#pragma once
#include "Enemy.h"

class CBossEnemy : public CEnemy
{
public:
	using CEnemy::CEnemy;

protected:
	bool UsesMonsterAI() const override { return false; }
};
