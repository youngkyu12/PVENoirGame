#include "pch.h"
#include "Player.h"

void Player::Update(uint32 serverTick)
{
	if (GetAnimState() == Protocol::ANIMATION_TYPE_DIE)
	{
		SetVelocity(GameMath::Vec3::Zero());

		constexpr uint32 kRespawnDelayTicks = 80; // ~5ÃÊ (60ms * 80)
		if (serverTick >= m_deathTick + kRespawnDelayTicks)
		{
			Respawn(serverTick);
		}
		return;
	}

	Move(m_velocity * ((GetAnimState() == Protocol::ANIMATION_TYPE_RUN) + 1));

	SetVelocity(GameMath::Vec3::Zero());

	if (m_animState != Protocol::ANIMATION_TYPE_IDLE)
	{
		int animDuration = 0;
		switch (m_animState)
		{
		case Protocol::ANIMATION_TYPE_WALK:
			animDuration = 15;
			break;
		case Protocol::ANIMATION_TYPE_RUN:
			animDuration = 10;
			break;
		case Protocol::ANIMATION_TYPE_ATTACK:
			animDuration = 10;
			break;
		case Protocol::ANIMATION_TYPE_ROLL:
			animDuration = 1;
			break;
		case Protocol::ANIMATION_TYPE_DIE:
			animDuration = 25;
			break;
		case Protocol::ANIMATION_TYPE_HIT:
			animDuration = 10;
			break;
		default:
			animDuration = 0;
			break;
		}
		if (animDuration > 0)
		{
			int elapsedTicks = serverTick - GetAnimTick();
			if (elapsedTicks >= animDuration && m_animState != Protocol::ANIMATION_TYPE_DIE
				&& m_animState != Protocol::ANIMATION_TYPE_WALK
				&& m_animState != Protocol::ANIMATION_TYPE_RUN)
			{
				SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
				SetAnimTick(serverTick);
			}
		}
	}
}

void Player::Build()
{
	SetPosition(0.0f, 0.0f, 0.0f);
	Rotate(0.0f, 0.0f, 0.0f);

	weapon.SetWeapon(Protocol::WEAPON_TYPE_SWORD, 0);
}

void Player::ApplyHit(uint32 serverTick, int damage, uint32 hitDurationTicks)
{
	if (IsDead()) return;

	TakeDamage(damage);

	if (IsDead())
	{
		cout << "Player " << GetObjectId() << " died" << endl;
		SetAnimState(Protocol::ANIMATION_TYPE_DIE);
		SetAnimTick(serverTick);
		SetVelocity(GameMath::Vec3::Zero());
		m_deathTick = serverTick;
		return;
	}

	cout << "Player " << GetObjectId() << " hit (HP: " << GetCurrentHp() << "/" << GetMaxHp() << ")" << endl;
	SetAnimState(Protocol::ANIMATION_TYPE_HIT);
	SetAnimTick(serverTick);
}

void Player::Respawn(uint32 serverTick)
{
	cout << "Player " << GetObjectId() << " respawned" << endl;
	ResetHpToMax();
	SetPosition(GameMath::Vec3(0.0f, 0.0f, -200.0f));
	SetVelocity(GameMath::Vec3::Zero());
	SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
	SetAnimTick(serverTick);
}