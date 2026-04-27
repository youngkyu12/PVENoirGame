#include "pch.h"
#include "Room.h"
#include "Player.h"

void Room::ProcessInput(uint64 playerId, int32 keyCodes, float deltaX, float deltaY)
{
	auto it = players.find(playerId);
	if (it == players.end())
		return;

	PlayerRef& player = it->second;

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
			FireArrow(player);
			break;
		case Protocol::WEAPON_TYPE_CANON:
			FireCannonball(player);
			break;
		default:
			player->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
			break;
		}
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

	const float speed = 5.0f;
	const float dt = 0.06f;
	float fDistance = speed * dt;

	GameMath::Vec3 shift = GameMath::Vec3::Zero();

	if (keyCodes & kDirForward)
		shift += look * fDistance;

	if (keyCodes & kDirBackward)
		shift += look * (-fDistance);

	if (keyCodes & kDirRight)
		shift += right * fDistance * 0.5f;

	if (keyCodes & kDirLeft)
		shift += right * (-fDistance) * 0.5f;

	const float moveMul = (player->GetAnimState() == Protocol::ANIMATION_TYPE_RUN) ? 2.0f : 1.0f;
	GameMath::Vec3 desiredShift = shift * moveMul;

	if (GameMath::Vec3::Dot(desiredShift, desiredShift) > 1e-8f)
		desiredShift = ResolvePreBlockedShift(player, desiredShift);

	shift = desiredShift / moveMul;
	player->SetVelocity(shift);
}
