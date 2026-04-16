#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "Enemy.h"
#include "Projectile.h"

void Room::TickAdvance()
{
	MakeFrameState(tick.load());

	for (auto player : players)
	{
		const GameMath::Vec3 prevPos = player.second->GetPosition();
		player.second->Update(tick);
		ResolveWorldStaticCollision(player.second, prevPos);
	}

	for (auto enemy : enemies)
	{
		const GameMath::Vec3 prevPos = enemy.second->GetPosition();
		enemy.second->Update(tick);
		ResolveWorldStaticCollision(enemy.second, prevPos);
	}

	for (auto& p : m_arrowPool)
	{
		if (!p->IsActive())
			continue;
		p->Update(tick);

		constexpr float kHitRadiusSq = 1.0f;
		for (auto& enemyPair : enemies)
		{
			auto& enemy = enemyPair.second;
			const GameMath::Vec3 d = enemy->GetPosition() - p->GetPosition();
			if (d.LengthSq() > kHitRadiusSq)
				continue;

			enemy->ApplyHit(tick.load(), 20);
			p->Deactivate();
			break;
		}
	}

	for (auto& p : m_bulletPool)
	{
		if (!p->IsActive())
			continue;
		p->Update(tick);

		constexpr float kHitRadiusSq = 1.0f;
		for (auto& enemyPair : enemies)
		{
			auto& enemy = enemyPair.second;
			const GameMath::Vec3 d = enemy->GetPosition() - p->GetPosition();
			if (d.LengthSq() > kHitRadiusSq)
				continue;

			enemy->ApplyHit(tick.load(), 20);
			p->Deactivate();
			break;
		}
	}

	if (_collision)
		_collision->OnUpdate();

	UpdateDynamicGridState();
	++tick;
}

ProjectileRef Room::AcquireFromPool(Vector<ProjectileRef>& pool)
{
	for (auto& p : pool)
	{
		if (!p->IsActive())
			return p;
	}

	return nullptr;
}

void Room::FireArrow(PlayerRef shooter)
{
	if (!shooter || !shooter->CanFire(tick.load()))
		return;

	auto p = AcquireFromPool(m_arrowPool);
	if (!p)
		return;

	const GameMath::Vec3 origin = shooter->GetPosition() + GameMath::Vec3(0.f, 1.5f, 0.f);
	const GameMath::Vec3 forward = shooter->GetLook().Normalized();
	constexpr float kArrowSpeed = 3.0f;
	constexpr int kArrowLifeTicks = 200;

	p->Activate(origin, forward * kArrowSpeed, kArrowLifeTicks, shooter->GetObjectId(), Protocol::BULLET_TYPE_ARROW);
	shooter->OnFired(tick.load());
	shooter->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
}

void Room::FireCannonball(PlayerRef shooter)
{
	if (!shooter || !shooter->CanFire(tick.load()))
		return;

	auto p = AcquireFromPool(m_bulletPool);
	if (!p)
		return;

	const GameMath::Vec3 origin = shooter->GetPosition() + GameMath::Vec3(0.f, 1.5f, 0.f);
	const GameMath::Vec3 forward = shooter->GetLook().Normalized();
	constexpr float kBulletSpeed = 10.0f;
	constexpr int kBulletLifeTicks = 100;

	p->Activate(origin, forward * kBulletSpeed, kBulletLifeTicks, shooter->GetObjectId(), Protocol::BULLET_TYPE_CANNONBALL);
	shooter->OnFired(tick.load());
	shooter->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
}
