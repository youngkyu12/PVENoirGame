#include "pch.h"
#include "Player.h"

void Player::Update(uint32 serverTick)
{
	Move(GetVelocity());

	// player는 Move 이후에는 자기 속도를 초기화하는 것으로 가정 (옵션)
	SetVelocity(GameMath::Vec3::Zero());

	// 애니메이션 업데이트 (옵션)

	if(m_animState != Protocol::ANIMATION_TYPE_IDLE)
	{
		int animDuration = 0;
		switch (m_animState)
		{
		case Protocol::ANIMATION_TYPE_WALK:
			animDuration = 30; // 30 ticks
			break;
		case Protocol::ANIMATION_TYPE_ATTACK:
			animDuration = 50; // 20 ticks
			break;
		case Protocol::ANIMATION_TYPE_DIE:
			animDuration = 50; // 50 ticks
			break;
		default:
			animDuration = 0;
			break;
		}
		if (animDuration > 0)
		{
			int elapsedTicks = serverTick - GetAnimTick(); // tick은 Room의 Atomic<uint32> tick
			if (elapsedTicks >= animDuration)
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
