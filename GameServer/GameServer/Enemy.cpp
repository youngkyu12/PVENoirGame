#include "pch.h"
#include "Enemy.h"
#include "MonsterAI.h"



void CEnemy::Build(GameMath::Vec3 pos, GameMath::Vec3 rot)
{
	SetPosition(pos);
	Rotate(rot.x, rot.y, rot.z);
}

void CEnemy::ApplyHit(uint32 serverTick, uint32 hitDurationTicks)
{
	SetAnimState(Protocol::ANIMATION_TYPE_HIT);
	SetAnimTick(serverTick);
	const uint32 endTick = serverTick + hitDurationTicks;
	if (endTick > m_hitEndTick)
		m_hitEndTick = endTick;
}

void CEnemy::Update(uint32 serverTick)
{
	if (GetAnimState() == Protocol::ANIMATION_TYPE_HIT)
	{
		if (serverTick < m_hitEndTick)
		{
			SetVelocity(GameMath::Vec3::Zero());
			CServerObject::Update(serverTick);
			return;
		}

		SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
		SetAnimTick(serverTick);
		m_hitEndTick = 0;
	}

	if (auto* ai = GetComponent<CMonsterAI>())
		ai->OnUpdate(0.03f);

	CServerObject::Update(serverTick);
}
