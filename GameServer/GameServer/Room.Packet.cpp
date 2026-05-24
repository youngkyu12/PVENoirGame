#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "Enemy.h"
#include "Projectile.h"
#include "GameSession.h"

#include "Protocol.pb.h"
#include "ClientPacketHandler.h"

#include <unordered_map>
#include <vector>

namespace
{
	enum : uint32
	{
		kStateMove = 1u << 0,
		kStateRight = 1u << 1,
		kStateLeft = 1u << 2,
		kStateDown = 1u << 3,
		kStateUp = 1u << 4,
		kStateAttack = 1u << 5,
		kStateRoll = 1u << 6,
		kStateRun = 1u << 7,
		kStateHit = 1u << 8,
		kStateDie = 1u << 9
	};

	enum : int32
	{
		kDirForward = 1 << 0,
		kDirBackward = 1 << 1,
		kDirLeft = 1 << 2,
		kDirRight = 1 << 3,
		kDirMoveMask = kDirForward | kDirBackward | kDirLeft | kDirRight
	};

	static bool CanMoveWhileAttacking(Protocol::WeaponType weaponType)
	{
		return weaponType == Protocol::WEAPON_TYPE_BOW ||
			weaponType == Protocol::WEAPON_TYPE_CANON;
	}

	static void AppendMoveKeyState(uint32& code, int32 moveKeyCodes)
	{
		if ((moveKeyCodes & kDirMoveMask) == 0)
			return;

		code |= kStateMove;
		if (moveKeyCodes & kDirForward) code |= kStateUp;
		if (moveKeyCodes & kDirBackward) code |= kStateDown;
		if (moveKeyCodes & kDirRight) code |= kStateRight;
		if (moveKeyCodes & kDirLeft) code |= kStateLeft;
	}

	static uint32 BuildStateCode(const Player& player)
	{
		uint32 code = 0;
		const auto anim = player.GetAnimState();

		if (anim == Protocol::ANIMATION_TYPE_DIE)
		{
			code |= kStateDie;
			return code; // Dead objects do not send movement flags.
		}

		if (anim == Protocol::ANIMATION_TYPE_ATTACK) code |= kStateAttack;
		if (anim == Protocol::ANIMATION_TYPE_ROLL)   code |= kStateRoll;
		if (anim == Protocol::ANIMATION_TYPE_RUN)    code |= kStateRun;
		if (anim == Protocol::ANIMATION_TYPE_HIT)    code |= kStateHit;

		if (anim == Protocol::ANIMATION_TYPE_HIT ||
			anim == Protocol::ANIMATION_TYPE_IDLE)
		{
			return code;
		}

		if (anim == Protocol::ANIMATION_TYPE_ATTACK &&
			!CanMoveWhileAttacking(player.GetWeaponState()))
		{
			return code;
		}

		const int32 moveKeyCodes =
			(anim == Protocol::ANIMATION_TYPE_ROLL)
			? player.GetRollMoveKeyCodes()
			: player.GetLastMoveKeyCodes();
		AppendMoveKeyState(code, moveKeyCodes);

		return code;
	}

	static uint32 BuildEnemyStateCode(const CServerObject& obj)
	{
		uint32 code = 0;
		const auto anim = obj.GetAnimState();

		if (anim == Protocol::ANIMATION_TYPE_DIE) code |= kStateDie;
		if (anim == Protocol::ANIMATION_TYPE_ATTACK) code |= kStateAttack;
		if (anim == Protocol::ANIMATION_TYPE_ROLL) code |= kStateRoll;
		if (anim == Protocol::ANIMATION_TYPE_RUN) code |= kStateRun;
		if (anim == Protocol::ANIMATION_TYPE_HIT) code |= kStateHit;

		if (anim == Protocol::ANIMATION_TYPE_RUN)
		{
			code |= kStateMove;

			constexpr float kEps = 1e-4f;
			const GameMath::Vec3& moveDir = obj.GetLastMoveDir();
			const float fwd = GameMath::Vec3::Dot(moveDir, obj.GetLook());
			const float str = GameMath::Vec3::Dot(moveDir, obj.GetRight());

			if (fwd > kEps)  code |= kStateUp;
			if (fwd < -kEps) code |= kStateDown;
			if (str > kEps)  code |= kStateRight;
			if (str < -kEps) code |= kStateLeft;
		}
		return code;
	}
}

void Room::FrameStateAdvance()
{
	const auto frameStart = std::chrono::steady_clock::now();

	MakeFrameState(tick.load());

	const auto elapsedMs = static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - frameStart).count());
	const uint64 frameStateIntervalMs = m_timing.frameStateIntervalMs;
	const uint64 nextDelayMs =
		(elapsedMs >= frameStateIntervalMs) ? 0 : (frameStateIntervalMs - elapsedMs);
	if (nextDelayMs == 0)
	{
		cout << "[FrameStateAdvance] next frame immediately (elapsed=" << elapsedMs << "ms)" << endl;
		GRoom->DoAsync(&Room::FrameStateAdvance);
	}
	else
	{
		GRoom->DoTimer(nextDelayMs, &Room::FrameStateAdvance);
	}
}

void Room::MakeFrameState(uint32 tick)
{
	constexpr float kEnemyViewRange = 200.0f;
	constexpr float kEnemyViewRangeSq = kEnemyViewRange * kEnemyViewRange;
	constexpr float kBulletViewRange = 100.0f;
	constexpr float kBulletViewRangeSq = kBulletViewRange * kBulletViewRange;

	std::unordered_map<uint64, uint32> s_EnemyStateCodeCache;
	RebuildMegaGridEnemyIds();

	for (auto& viewerPair : players)
	{
		PlayerRef& viewer = viewerPair.second;
		if (!viewer || !viewer->ownerSession)
			continue;

		Protocol::S_FRAME_STATE frameStatePkt;
		frameStatePkt.set_servertick(tick);

		for (auto& playerMap : players)
		{
			PlayerRef& player = playerMap.second;
			if (!player)
				continue;

			auto p = frameStatePkt.add_players();
			p->set_id(player->playerId);
			p->set_name(player->name);
			p->set_playertype(player->type);

			Protocol::Animation* anim = p->mutable_animation();
			anim->set_statecode(BuildStateCode(*player));
			anim->set_animationtick(player->GetAnimTick());

			p->set_weapontype(player->GetWeaponState());

			Protocol::Transform* transform = p->mutable_transform();
			Protocol::Vec3f* position = transform->mutable_position();
			position->set_x(player->GetPosition().x);
			position->set_y(player->GetPosition().y);
			position->set_z(player->GetPosition().z);
			transform->set_yaw(player->GetYaw());
		}

		const GameMath::Vec3 viewerPos = viewer->GetPosition();

		std::vector<uint64> visibleEnemyIds;
		CollectEnemyIdsInMegaGridRadius(viewerPos, kEnemyViewRange, visibleEnemyIds);

		for (uint64 enemyId : visibleEnemyIds)
		{
			auto it = enemies.find(enemyId);
			if (it == enemies.end())
				continue;

			EnemyRef& enemy = it->second;
			if (!enemy)
				continue;

			if (GameMath::DistSqXZ(viewerPos, enemy->GetPosition()) > kEnemyViewRangeSq)
				continue;

			if (s_EnemyStateCodeCache.find(enemyId) == s_EnemyStateCodeCache.end())
				s_EnemyStateCodeCache[enemyId] = BuildEnemyStateCode(*enemy);

			auto e = frameStatePkt.add_enemies();
			e->set_id(enemyId);
			e->set_enemytype(enemy->type);
			e->set_weapontype(enemy->GetWeaponState());
			Protocol::Animation* anim = e->mutable_animation();
			anim->set_statecode(s_EnemyStateCodeCache[enemyId]);
			anim->set_animationtick(enemy->GetAnimTick());

			Protocol::Transform* transform = e->mutable_transform();
			Protocol::Vec3f* position = transform->mutable_position();
			position->set_x(enemy->GetPosition().x);
			position->set_y(enemy->GetPosition().y);
			position->set_z(enemy->GetPosition().z);
			transform->set_yaw(enemy->GetYaw());
		}

		auto AddVisibleBullet = [&](ProjectileRef projectile)
			{
				if (!projectile || !projectile->IsActive())
					return;

				if (GameMath::DistSqXZ(viewerPos, projectile->GetPosition()) > kBulletViewRangeSq)
					return;

				Protocol::Bullet* b = frameStatePkt.add_bullets();
				b->set_id(projectile->GetObjectId());
				b->set_ownerid(projectile->GetOwnerId());
				b->set_bullettype(projectile->GetBulletType());

				Protocol::Vec3f* pos = b->mutable_position();
				pos->set_x(projectile->GetPosition().x);
				pos->set_y(projectile->GetPosition().y);
				pos->set_z(projectile->GetPosition().z);

				Protocol::Vec3f* vel = b->mutable_velocity();
				vel->set_x(projectile->GetVelocity().x);
				vel->set_y(projectile->GetVelocity().y);
				vel->set_z(projectile->GetVelocity().z);
			};

		for (auto& projectile : m_arrowPool)
			AddVisibleBullet(projectile);

		for (auto& projectile : m_bulletPool)
			AddVisibleBullet(projectile);

		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(frameStatePkt);
		viewer->ownerSession->Send(sendBuffer);
	}



}

void Room::SendTowerPortalForcedYawDelta(const PlayerRef& player, float yawDelta)
{
	if (!player || !player->ownerSession)
		return;

	Protocol::S_FORCED_TRANSFORM pkt;
	pkt.set_playerid(player->playerId);
	pkt.set_yawdelta(yawDelta);
	pkt.set_reason(Protocol::FORCED_TRANSFORM_REASON_TOWER_PORTAL);

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(pkt);
	player->ownerSession->Send(sendBuffer);
}

void Room::MakeInitStruct(Protocol::S_GAME_START gameStartPkt)
{
	gameStartPkt.set_mapid("MapData_fullstage_withBoss");

	Protocol::InitStruct* initStruct = gameStartPkt.mutable_initstruct();
	for (auto playerMap : players)
	{
		PlayerRef& player = playerMap.second;

		auto p = initStruct->add_players();
		p->set_id(player->playerId);
		p->set_name(player->name);
		p->set_playertype(player->type);

		Protocol::Transform* transform = p->mutable_transform();
		Protocol::Vec3f* position = transform->mutable_position();
		position->set_x(player->GetPosition().x);
		position->set_y(player->GetPosition().y);
		position->set_z(player->GetPosition().z);
		transform->set_yaw(player->GetYaw());
	}

	for (auto enemyMap : enemies)
	{
		EnemyRef& enemy = enemyMap.second;
		auto e = initStruct->add_enemies();
		e->set_id(enemyMap.first);
		e->set_enemytype(enemy->type);

		Protocol::Transform* transform = e->mutable_transform();
		Protocol::Vec3f* position = transform->mutable_position();
		position->set_x(enemy->GetPosition().x);
		position->set_y(enemy->GetPosition().y);
		position->set_z(enemy->GetPosition().z);
		transform->set_yaw(enemy->GetYaw());
	}

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(gameStartPkt);
	GRoom->DoAsync(&Room::BroadCastAll, sendBuffer);

	for (auto& player : players)
		player.second->SetActive(false);

	GRoom->DoTimer(m_timing.clientReadyPollIntervalMs, &Room::CheckClientReady);
}

void Room::MakeEnterGameStruct(Protocol::S_ENTER_GAME enterGamePkt)
{
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(enterGamePkt);
	BroadCastAll(sendBuffer);
}
