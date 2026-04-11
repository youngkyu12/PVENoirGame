#include "pch.h"
#include "Enemy.h"
#include "MonsterAI.h"



void CEnemy::Build(GameMath::Vec3 pos, GameMath::Vec3 rot)
{
	SetPosition(pos);
	Rotate(rot.x, rot.y, rot.z);
}

void CEnemy::Update(uint32 serverTick)
{
	if (auto* ai = GetComponent<CMonsterAI>())
		ai->OnUpdate(0.03f);

	CServerObject::Update(serverTick);
}
