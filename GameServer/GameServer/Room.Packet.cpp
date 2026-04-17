#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "Enemy.h"
#include "Projectile.h"
#include "GameSession.h"

#include "Protocol.pb.h"
#include "ClientPacketHandler.h"

#include <unordered_map>

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

	static uint32 BuildStateCode(const CServerObject& obj)
	{
		uint32 code = 0;
		const auto anim = obj.GetAnimState();

		if (anim == Protocol::ANIMATION_TYPE_DIE) code |= kStateDie;
		if (anim == Protocol::ANIMATION_TYPE_ATTACK) code |= kStateAttack;
		if (anim == Protocol::ANIMATION_TYPE_ROLL) code |= kStateRoll;
		if (anim == Protocol::ANIMATION_TYPE_RUN) code |= kStateRun;
		if (anim == Protocol::ANIMATION_TYPE_HIT) code |= kStateHit;

		const GameMath::Vec3 v = obj.GetVelocity();
		constexpr float kEps = 1e-4f;

		if (v.LengthSq() > kEps)
		{
			code |= kStateMove;

			const float fwd = GameMath::Vec3::Dot(v, obj.GetLook());
			const float str = GameMath::Vec3::Dot(v, obj.GetRight());

			if (fwd > kEps) code |= kStateUp;
			if (fwd < -kEps) code |= kStateDown;
			if (str > kEps) code |= kStateRight;
			if (str < -kEps) code |= kStateLeft;
		}

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

		static std::unordered_map<uint64, GameMath::Vec3> s_prevEnemyPos;

		const uint64 enemyId = obj.GetObjectId();
		const GameMath::Vec3 curPos = obj.GetPosition();
		constexpr float kEps = 1e-4f;

		auto it = s_prevEnemyPos.find(enemyId);
		if (it != s_prevEnemyPos.end())
		{
			const GameMath::Vec3& prevPos = it->second;
			GameMath::Vec3 delta(curPos.x - prevPos.x, 0.0f, curPos.z - prevPos.z);

			if (delta.LengthSq() > kEps)
			{
				code |= kStateMove;

				const float fwd = GameMath::Vec3::Dot(delta, obj.GetLook());
				const float str = GameMath::Vec3::Dot(delta, obj.GetRight());

				if (fwd > kEps) code |= kStateUp;
				if (fwd < -kEps) code |= kStateDown;
				if (str > kEps) code |= kStateRight;
				if (str < -kEps) code |= kStateLeft;
			}
		}

		s_prevEnemyPos[enemyId] = curPos;
		return code;
	}
}

void Room::MakeFrameState(uint32 tick)
{
	constexpr float kEnemyViewRange = 200.0f;
	constexpr float kEnemyViewRangeSq = kEnemyViewRange * kEnemyViewRange;
	constexpr float kBulletViewRange = 100.0f;
	constexpr float kBulletViewRangeSq = kBulletViewRange * kBulletViewRange;

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

		for (auto& enemyMap : enemies)
		{
			EnemyRef& enemy = enemyMap.second;
			if (!enemy)
				continue;

			if (DistSqXZ(viewerPos, enemy->GetPosition()) > kEnemyViewRangeSq)
				continue;

			auto e = frameStatePkt.add_enemies();
			e->set_id(enemyMap.first);
			e->set_enemytype(enemy->type);
			e->set_weapontype(enemy->GetWeaponState());

			Protocol::Animation* anim = e->mutable_animation();
			anim->set_statecode(BuildEnemyStateCode(*enemy));
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

				if (DistSqXZ(viewerPos, projectile->GetPosition()) > kBulletViewRangeSq)
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

void Room::MakeInitStruct(Protocol::S_GAME_START gameStartPkt)
{
	gameStartPkt.set_mapid("MapData_fullstage");

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

	CheckClientReady();
}

void Room::MakeEnterGameStruct(Protocol::S_ENTER_GAME enterGamePkt)
{
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(enterGamePkt);
	BroadCastAll(sendBuffer);
}
