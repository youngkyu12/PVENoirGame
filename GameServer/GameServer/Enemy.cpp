#include "pch.h"
#include "Enemy.h"
#include "MonsterAI.h"

CEnemy::~CEnemy() = default;

void CEnemy::EnsureAI()
{
	if (!m_monsterAI)
		m_monsterAI = std::make_unique<CMonsterAI>(this);
}


void CEnemy::Build(GameMath::Vec3 pos, GameMath::Vec3 rot)
{
	EnsureAI();
	SetPosition(pos);
	Rotate(rot.x, rot.y, rot.z);
}

void CEnemy::UpdateAI(float dt)
{
	EnsureAI();
	if (m_monsterAI)
		m_monsterAI->OnUpdate(dt);
}

void CEnemy::ApplyHit(uint32 serverTick, int damage, uint32 hitDurationTicks)
{
	if (IsDead()) return;

	TakeDamage(damage);

	if (IsDead())
	{
		SetAnimState(Protocol::ANIMATION_TYPE_DIE);
		SetAnimTick(serverTick);
		SetVelocity(GameMath::Vec3::Zero());
		m_hitEndTick = 0;
		return;
	}

	SetAnimState(Protocol::ANIMATION_TYPE_HIT);
	SetAnimTick(serverTick);
	const uint32 endTick = serverTick + hitDurationTicks;
	if (endTick > m_hitEndTick)
		m_hitEndTick = endTick;
}

void CEnemy::Update(uint32 serverTick)
{
	if (GetAnimState() == Protocol::ANIMATION_TYPE_DIE)
	{
		SetVelocity(GameMath::Vec3::Zero());
		return;
	}

	if (GetAnimState() == Protocol::ANIMATION_TYPE_HIT)
	{
		if (serverTick < m_hitEndTick)
		{
			SetVelocity(GameMath::Vec3::Zero());
			ApplyPhysics(0.03f);
			return;
		}

		SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
		SetAnimTick(serverTick);
		m_hitEndTick = 0;
	}

	CServerObject::Update(serverTick);
}