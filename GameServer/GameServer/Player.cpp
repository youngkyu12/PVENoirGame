#include "pch.h"
#include "Player.h"

void Player::Update(uint32 serverTick)
{
	Move(m_velocity * ((GetAnimState() == Protocol::ANIMATION_TYPE_RUN) + 1));

	// player는 Move 이후에는 자기 속도를 초기화하는 것으로 가정 (옵션)
	SetVelocity(GameMath::Vec3::Zero());

	// 애니메이션 업데이트 (옵션)

	if(m_animState != Protocol::ANIMATION_TYPE_IDLE)
	{
		int animDuration = 0;
		switch (m_animState)
		{
		case Protocol::ANIMATION_TYPE_WALK:
			animDuration = 15; // 30 ticks
			break;
		case Protocol::ANIMATION_TYPE_RUN:
			animDuration = 10; // 20 ticks
			break;
		case Protocol::ANIMATION_TYPE_ATTACK:
			animDuration = 10; // 20 ticks
			break;
		case Protocol::ANIMATION_TYPE_ROLL:
			animDuration = 1; // 40 ticks
			break;
		case Protocol::ANIMATION_TYPE_DIE:
			animDuration = 25; // 50 ticks
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
			int elapsedTicks = serverTick - GetAnimTick(); // tick은 Room의 Atomic<uint32> tick
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

void Player::ApplyHit(uint32 serverTick, uint32 hitDurationTicks)
{
	cout << "Player Hit" << endl;
	SetAnimState(Protocol::ANIMATION_TYPE_HIT);
	SetAnimTick(serverTick);
}