#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "Enemy.h"
#include "Projectile.h"

#include <algorithm>
#include <chrono>
#include <vector>

namespace
{
	static bool IsEnemyNearAnyPlayer(const map<uint64, PlayerRef>& players, const GameMath::Vec3& enemyPos, float rangeSq)
	{
		for (const auto& playerPair : players)
		{
			const PlayerRef& player = playerPair.second;
			if (!player)
				continue;

			if (DistSqXZ(player->GetPosition(), enemyPos) <= rangeSq)
				return true;
		}

		return false;
	}

	static void UpdateEnemyAIChunk(const std::vector<EnemyRef>& activeEnemies, size_t beginIndex, size_t endIndex, float dt)
	{
		for (size_t i = beginIndex; i < endIndex; ++i)
		{
			const EnemyRef& enemy = activeEnemies[i];
			if (!enemy)
				continue;

			enemy->UpdateAI(dt);
		}
	}
}

void Room::ProcessEnemyAI()
{
	const auto frameStart = std::chrono::steady_clock::now();


	constexpr float kEnemyAiActiveRange = 100.0f;
	constexpr float kEnemyAiActiveRangeSq = kEnemyAiActiveRange * kEnemyAiActiveRange;
	constexpr float kFixedDtSec = 0.06f;
	constexpr size_t kEnemyAiChunkSize = 32;

	std::vector<EnemyRef> activeEnemies;
	activeEnemies.reserve(enemies.size());

	for (auto& enemyPair : enemies)
	{
		auto& enemy = enemyPair.second;
		if (!enemy)
			continue;

		if (IsEnemyNearAnyPlayer(players, enemy->GetPosition(), kEnemyAiActiveRangeSq))
			activeEnemies.push_back(enemy);
		else
			enemy->SetVelocity(GameMath::Vec3::Zero());
	}

	for (size_t beginIndex = 0; beginIndex < activeEnemies.size(); beginIndex += kEnemyAiChunkSize)
	{
		const size_t endIndex = (std::min)(beginIndex + kEnemyAiChunkSize, activeEnemies.size());
		UpdateEnemyAIChunk(activeEnemies, beginIndex, endIndex, kFixedDtSec);
	}

	const auto elapsedMs = static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - frameStart).count());
	const uint64 nextDelayMs = (elapsedMs >= 60) ? 0 : (60 - elapsedMs);
	GRoom->DoTimer(nextDelayMs, &Room::ProcessEnemyAI);
}



void Room::TickAdvance()
{
	const auto frameStart = std::chrono::steady_clock::now();

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
	const auto elapsedMs = static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - frameStart).count());
	const uint64 nextDelayMs = (elapsedMs >= 60) ? 0 : (60 - elapsedMs);
	GRoom->DoTimer(nextDelayMs, &Room::TickAdvance);
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
