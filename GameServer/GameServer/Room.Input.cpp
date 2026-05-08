#include "pch.h"
#include "Room.h"
#include "Player.h"

void Room::ProcessInput(uint64 playerId, int32 keyCodes, float deltaX, float deltaY)
{
	auto it = players.find(playerId);
	if (it == players.end())
		return;

	PlayerRef& player = it->second;
	if (!player)
		return;

	// [추가] 죽음/리스폰 중 입력 차단
	if (player->IsDead() || player->IsInputBlocked())
	{
		player->SetVelocity(GameMath::Vec3::Zero());
		return;
	}


	if (deltaX != 0.0f)
	{
		float currentYaw = player->GetYaw();
		player->SetYaw(GameMath::NormalizeYaw(currentYaw + deltaX));
	}

	constexpr int kDirForward = 1 << 0;
	constexpr int kDirBackward = 1 << 1;
	constexpr int kDirLeft = 1 << 2;
	constexpr int kDirRight = 1 << 3;
	constexpr int kDirLButton = 1 << 7;
	constexpr int kDirRun = 1 << 8;
	constexpr int kDirRoll = 1 << 9;

	Protocol::AnimationType prevAnimState = player->GetAnimState();

	if ((keyCodes & kDirLButton) != 0)
	{
		switch (player->GetWeaponState())
		{
		case Protocol::WEAPON_TYPE_BOW:
			if (player->GetWeapon().BeginAttack(tick.load()))
			{
				player->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
				player->SetAnimTick(tick.load());
				//player->SetVelocity(GameMath::Vec3::Zero());
			}
			break;
		case Protocol::WEAPON_TYPE_CANON:
			FireCannonball(player);
			break;
		case Protocol::WEAPON_TYPE_SWORD:
		case Protocol::WEAPON_TYPE_AXE:
			player->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
			player->SetVelocity(GameMath::Vec3::Zero());
			break;
		default:
			player->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
			break;
		}

		// 공격 애니메이션이 시작되면 이동 입력은 무시되어야 한다
	}
	else if (prevAnimState != Protocol::ANIMATION_TYPE_ATTACK &&
		prevAnimState != Protocol::ANIMATION_TYPE_ROLL &&
		prevAnimState != Protocol::ANIMATION_TYPE_HIT)
	{
		player->SetAnimState(keyCodes & (kDirForward | kDirBackward | kDirLeft | kDirRight) ?
			(keyCodes & kDirRun ? Protocol::ANIMATION_TYPE_RUN : Protocol::ANIMATION_TYPE_WALK) :
			Protocol::ANIMATION_TYPE_IDLE);
	}

	player->SetAnimState(keyCodes & (kDirForward | kDirBackward | kDirLeft | kDirRight) &&
		keyCodes & kDirRoll
		&& (prevAnimState != Protocol::ANIMATION_TYPE_ROLL)
		? Protocol::ANIMATION_TYPE_ROLL : player->GetAnimState());

	if (player->GetAnimState() != prevAnimState)
		player->SetAnimTick(tick);

	GameMath::Vec3 look = player->GetLook();
	GameMath::Vec3 right = player->GetRight();

	const float speed = 8.0f;
	const float dt = 0.06f;
	float fDistance = speed * dt;

	GameMath::Vec3 shift = GameMath::Vec3::Zero();
	GameMath::Vec3 moveDirection = GameMath::Vec3::Zero();

	// 이동 방향에 따라 fdistaance 조절
	if (keyCodes & kDirForward)
	{
		moveDirection += look;
	}

	if (keyCodes & kDirBackward)
	{
		moveDirection -= look;
	}

	if (keyCodes & kDirRight)
	{
		moveDirection += right;
		fDistance *= 0.5f;
	}

	if (keyCodes & kDirLeft)
	{
		moveDirection -= right;
		fDistance *= 0.5f;
	}

	switch (player->GetAnimState())
	{

		case Protocol::ANIMATION_TYPE_ROLL:
		{
			// 구르기는 기존 상태를 계속 유지
			break;
		}
		case Protocol::ANIMATION_TYPE_ATTACK:
		{
			const bool canMoveWhileAttacking =
				(player->GetWeaponState() == Protocol::WEAPON_TYPE_BOW ||
				 player->GetWeaponState() == Protocol::WEAPON_TYPE_CANON);
			if (!canMoveWhileAttacking)
				fDistance *= 0.0f; // 공격 도중에는 이동 속도 = 0
			break;
		}
		case Protocol::ANIMATION_TYPE_IDLE:
		{
			// IDLE도 이동하면 안됨
			fDistance *= 0.0f;
			break;
		}
		case Protocol::ANIMATION_TYPE_RUN:
		{
			fDistance *= 2.0f;
		}
		case Protocol::ANIMATION_TYPE_WALK:
		{


			break;
		}
		default:
		{
			break;
		}
	}

	if(player->GetAnimState() == Protocol::ANIMATION_TYPE_ROLL)
	{
		// 구르기는 이동 방향이 고정되어야 한다
		if (prevAnimState == Protocol::ANIMATION_TYPE_IDLE || prevAnimState == Protocol::ANIMATION_TYPE_WALK || prevAnimState == Protocol::ANIMATION_TYPE_RUN)
		{
			// 구르기가 시작된 시점으로, 당시 입력된 방향키 기준으로 조정함
			// 이미 방향키 정보 반영은 앞에서 했다. 넘긴다
			

			//moveDirection = player->GetLook();
		}
		else
		{
			// 원래의 속도/방향을 유지
			moveDirection = player->GetVelocity().Normalized();
		}
	}
	shift += moveDirection * fDistance;

	const float moveMul = (player->GetAnimState() == Protocol::ANIMATION_TYPE_RUN
		&& !(player->GetAnimState() == Protocol::ANIMATION_TYPE_ROLL 
			|| player->GetAnimState() == Protocol::ANIMATION_TYPE_ATTACK)) 
		? 2.0f : 1.0f;
	GameMath::Vec3 desiredShift = shift * moveMul;

	if (GameMath::Vec3::Dot(desiredShift, desiredShift) > 1e-8f)
		desiredShift = ResolvePreBlockedShift(player, desiredShift);

	shift = desiredShift / moveMul;
	player->SetVelocity(shift);
}
