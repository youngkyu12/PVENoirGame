#include "pch.h"
#include "Room.h"
#include "Player.h"

namespace
{
	constexpr int kDirForward        = 1 << 0;
	constexpr int kDirBackward       = 1 << 1;
	constexpr int kDirLeft           = 1 << 2;
	constexpr int kDirRight          = 1 << 3;
	constexpr int kDirLButton        = 1 << 7;
	constexpr int kDirRun            = 1 << 8;
	constexpr int kDirRoll           = 1 << 9;
	constexpr int kDirMoveMask       = kDirForward | kDirBackward | kDirLeft | kDirRight;
	constexpr int kDirPacketMoveMask = kDirMoveMask | kDirRun;

	constexpr float kPlayerWalkSpeed     = 5.0f;
	constexpr float kPlayerRollSpeed     = 5.0f;
	constexpr float kPlayerRunMultiplier = 2.0f;
}

void Room::ProcessInput(uint64 playerId, int32 keyCodes, float deltaX, float deltaY, float clientDeltaTime)
{
	auto it = players.find(playerId);
	if (it == players.end())
		return;

	PlayerRef& player = it->second;
	if (!player)
		return;

	if (player->HasPendingPortalTeleport())
	{
		player->SetVelocity(GameMath::Vec3::Zero());
		player->ClearMoveKeyCodes();
		return;
	}

	// [�߰�] ����/������ �� �Է� ����
	if (player->IsDead() || player->IsInputBlocked())
	{
		player->SetVelocity(GameMath::Vec3::Zero());
		player->ClearMoveKeyCodes();
		return;
	}


	if (deltaX != 0.0f)
	{
		float currentYaw = player->GetYaw();
		player->SetYaw(GameMath::NormalizeYaw(currentYaw + deltaX));
	}

	const int32 moveKeyCodes = keyCodes & kDirMoveMask;
	const int32 packetMoveKeyCodes = (moveKeyCodes != 0) ? (keyCodes & kDirPacketMoveMask) : 0;
	player->SetLastMoveKeyCodes(packetMoveKeyCodes);

	const uint32 animClockTick = GetAnimClockTick();
	const uint32 combatClockTick = GetCombatClockTick();
	Protocol::AnimationType prevAnimState = player->GetAnimState();

	if (prevAnimState == Protocol::ANIMATION_TYPE_ROLL)
		return;

	if ((keyCodes & kDirLButton) != 0)
	{
		switch (player->GetWeaponState())
		{
		case Protocol::WEAPON_TYPE_BOW:
			if (player->GetWeapon().BeginAttack(combatClockTick))
			{
				player->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
				player->SetAnimTick(animClockTick);
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

		// ���� �ִϸ��̼��� ���۵Ǹ� �̵� �Է��� ���õǾ�� �Ѵ�
	}
	else if (prevAnimState != Protocol::ANIMATION_TYPE_ATTACK &&
		prevAnimState != Protocol::ANIMATION_TYPE_ROLL &&
		prevAnimState != Protocol::ANIMATION_TYPE_HIT)
	{
		player->SetAnimState(moveKeyCodes != 0 ?
			(keyCodes & kDirRun ? Protocol::ANIMATION_TYPE_RUN : Protocol::ANIMATION_TYPE_WALK) :
			Protocol::ANIMATION_TYPE_IDLE);
	}

	const bool rollStarted = (keyCodes & kDirRoll) != 0 &&
		prevAnimState != Protocol::ANIMATION_TYPE_ROLL;
	if (rollStarted)
	{
		player->SetRollMoveKeyCodes(packetMoveKeyCodes);
		player->SetAnimState(Protocol::ANIMATION_TYPE_ROLL);
	}

	if (player->GetAnimState() != prevAnimState)
		player->SetAnimTick(animClockTick);

	GameMath::Vec3 look = player->GetLook();
	GameMath::Vec3 right = player->GetRight();

	float dt = clientDeltaTime;
	if (!(dt > 0.0f))
		dt = m_timing.playerInputDtSec;
	if (dt < 0.001f)
		dt = 0.001f;
	if (dt > 0.05f)
		dt = 0.05f;
	float fDistance = kPlayerWalkSpeed * dt;

	GameMath::Vec3 shift = GameMath::Vec3::Zero();
	GameMath::Vec3 moveDirection = GameMath::Vec3::Zero();

	// �̵� ���⿡ ���� fdistaance ����
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
	}

	if (keyCodes & kDirLeft)
	{
		moveDirection -= right;
	}

	if (GameMath::Vec3::Dot(moveDirection, moveDirection) > 1e-8f)
		moveDirection = moveDirection.Normalized();

	switch (player->GetAnimState())
	{

		case Protocol::ANIMATION_TYPE_ROLL:
		{
			fDistance = kPlayerRollSpeed * m_timing.playerInputDtSec;
			break;
		}
		case Protocol::ANIMATION_TYPE_ATTACK:
		{
			const bool canMoveWhileAttacking =
				(player->GetWeaponState() == Protocol::WEAPON_TYPE_BOW ||
				 player->GetWeaponState() == Protocol::WEAPON_TYPE_CANON);
			if (!canMoveWhileAttacking)
				fDistance *= 0.0f; // ���� ���߿��� �̵� �ӵ� = 0
			break;
		}
		case Protocol::ANIMATION_TYPE_IDLE:
		{
			// IDLE�� �̵��ϸ� �ȵ�
			fDistance *= 0.0f;
			break;
		}
		case Protocol::ANIMATION_TYPE_RUN:
		{
			fDistance *= kPlayerRunMultiplier;
			break;
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

	if (player->GetAnimState() == Protocol::ANIMATION_TYPE_ROLL)
	{
		if (GameMath::Vec3::Dot(moveDirection, moveDirection) <= 1e-8f)
			moveDirection = player->GetLook().Normalized();
	}
	shift += moveDirection * fDistance;

	GameMath::Vec3 desiredShift = shift;

	if (GameMath::Vec3::Dot(desiredShift, desiredShift) > 1e-8f)
	{
		if (TryQueuePortalTeleportFromBlockedMove(player, desiredShift))
		{
			player->SetVelocity(GameMath::Vec3::Zero());
			player->ClearMoveKeyCodes();
			return;
		}

		desiredShift = ResolvePreBlockedShift(player, desiredShift);
	}

	if (rollStarted)
		player->SetVelocity(desiredShift);
	else
		player->SetVelocity(player->GetVelocity() + desiredShift);

	//cout << "=====" << player->GetObjectId() << " Input Access!" << 
	//	 "=====" << endl << endl;
	//cout << "desiredShift:" << desiredShift.x << ", " << desiredShift.y << ", " << desiredShift.z << endl;
	//cout << "Client DeltaTime: " << clientDeltaTime << endl;
	//cout << "==============================================" << endl;
}
