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
	constexpr int kAtkPlayerSword = 10;
	constexpr int kAtkPlayerAxe = 15;
	constexpr int kAtkPlayerArrow = 15;
	constexpr int kAtkPlayerBullet = 8;

	int GetPlayerAttackPower(Protocol::WeaponType weapon)
	{
		switch (weapon)
		{
		case Protocol::WEAPON_TYPE_SWORD:  return kAtkPlayerSword;
		case Protocol::WEAPON_TYPE_AXE:    return kAtkPlayerAxe;
		case Protocol::WEAPON_TYPE_BOW:    return kAtkPlayerArrow;
		case Protocol::WEAPON_TYPE_CANON:  return kAtkPlayerBullet;
		default: return 5;
		}
	}

	static bool IsEnemyNearAnyPlayer(const map<uint64, PlayerRef>& players, const GameMath::Vec3& enemyPos, float rangeSq)
	{
		for (const auto& playerPair : players)
		{
			const PlayerRef& player = playerPair.second;
			if (!player) continue;
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
			if (!enemy) continue;
			enemy->UpdateAI(dt);
		}
	}

	bool IsInArcXZ(
		const GameMath::Vec3& attackerPos,
		const GameMath::Vec3& attackerLook,
		const GameMath::Vec3& targetPos,
		float reach,
		float halfAngleDeg)
	{
		const float dx = targetPos.x - attackerPos.x;
		const float dz = targetPos.z - attackerPos.z;
		const float distSq = dx * dx + dz * dz;

		if (distSq > reach * reach) return false;
		if (distSq < 1e-8f) return true;

		const float dist = sqrtf(distSq);
		const float dot = (dx / dist) * attackerLook.x + (dz / dist) * attackerLook.z;
		const float cosHalf = cosf(halfAngleDeg * GameMath::DEG_TO_RAD);

		return dot >= cosHalf;
	}
}

void Room::ProcessEnemyAI()
{
	const auto frameStart = std::chrono::steady_clock::now();

	constexpr float kEnemyAiActiveRange = 100.0f;
	constexpr float kEnemyAiActiveRangeSq = kEnemyAiActiveRange * kEnemyAiActiveRange;
	constexpr float kFixedDtSec = 0.3f;
	constexpr size_t kEnemyAiChunkSize = 32;

	std::vector<EnemyRef> activeEnemies;
	activeEnemies.reserve(enemies.size());

	for (auto& enemyPair : enemies)
	{
		auto& enemy = enemyPair.second;
		if (!enemy) continue;
		if (enemy->IsDead()) continue;

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
	const uint64 nextDelayMs = (elapsedMs >= 300) ? 0 : (300 - elapsedMs);
	if (nextDelayMs == 0)
	{
		cout << "Warning: Enemy AI processing is taking too long (" << elapsedMs << " ms)" 
			<< endl;
		GRoom->DoAsync(&Room::ProcessEnemyAI);
	}
	else
	{
		GRoom->DoTimer(nextDelayMs, &Room::ProcessEnemyAI);
	}
}

void Room::TickAdvance()
{
	const auto frameStart = std::chrono::steady_clock::now();


	for (auto player : players)
	{
		const GameMath::Vec3 prevPos = player.second->GetPosition();
		player.second->Update(tick);
		ResolveWorldStaticCollision(player.second, prevPos);
	}

	for (auto& [pid, player] : players)
	{
		if (!player) continue;
		if (player->IsDead()) continue;

		WeaponFireRequest req = player->GetWeapon().UpdateAttack(tick.load());
		if (!req.fire) continue;

		switch (req.bulletType)
		{
		case Protocol::BULLET_TYPE_ARROW:
			FireArrow(player, req.speed, req.lifeTicks);
			break;
		default:
			break;
		}
	}

	for (auto enemy : enemies)
	{
		const GameMath::Vec3 prevPos = enemy.second->GetPosition();
		enemy.second->Update(tick);
		ResolveWorldStaticCollision(enemy.second, prevPos);
	}

	// 화살 히트
	for (auto& p : m_arrowPool)
	{
		if (!p->IsActive()) continue;
		p->Update(tick);

		constexpr float kHitRadiusSq = 1.0f;
		for (auto& enemyPair : enemies)
		{
			auto& enemy = enemyPair.second;
			if (enemy->IsDead()) continue;
			//const GameMath::Vec3 d = enemy->GetPosition() - p->GetPosition();

			float distSq = GameMath::DistSqXZ(enemy->GetPosition(), p->GetPosition());
			const bool hit = distSq <= kHitRadiusSq
				&& enemy->GetPosition().y - p->GetPosition().y <= 1.0f;
			if (!hit) continue;

			enemy->ApplyHit(tick.load(), kAtkPlayerArrow, 20);
			p->Deactivate();
			break;
		}
	}

	// 포탄 히트
	for (auto& p : m_bulletPool)
	{
		if (!p->IsActive()) continue;
		p->Update(tick);

		constexpr float kHitRadiusSq = 1.0f;
		for (auto& enemyPair : enemies)
		{
			auto& enemy = enemyPair.second;
			if (enemy->IsDead()) continue;
			//const GameMath::Vec3 d = enemy->GetPosition() - p->GetPosition();

			float distSq = GameMath::DistSqXZ(enemy->GetPosition(), p->GetPosition());
			const bool hit = distSq <= kHitRadiusSq
				&& enemy->GetPosition().y - p->GetPosition().y <= 1.0f;
			if (!hit) continue;

			enemy->ApplyHit(tick.load(), kAtkPlayerBullet, 20);
			p->Deactivate();
			break;
		}
	}

	// 플레이어 근접 공격 히트 판정
	for (auto& [pid, player] : players)
	{
		if (player->IsDead()) continue;
		if (player->GetAnimState() != Protocol::ANIMATION_TYPE_ATTACK) continue;

		const auto weaponType = player->GetWeaponState();
		if (weaponType == Protocol::WEAPON_TYPE_BOW ||
			weaponType == Protocol::WEAPON_TYPE_CANON) continue;

		const int elapsed = static_cast<int>(tick.load()) - player->GetAnimTick();
		constexpr int kHitFrameStart = 5;
		constexpr int kHitFrameEnd = 15;
		if (elapsed < kHitFrameStart || elapsed > kHitFrameEnd) continue;

		const int damage = GetPlayerAttackPower(weaponType);

		float reach, halfAngleDeg;
		switch (weaponType)
		{
		case Protocol::WEAPON_TYPE_SWORD: reach = 2.0f; halfAngleDeg = 45.0f; break;
		case Protocol::WEAPON_TYPE_AXE:   reach = 2.5f; halfAngleDeg = 45.0f; break;
		default:                          reach = 1.5f; halfAngleDeg = 90.0f; break;
		}

		for (auto& [eid, enemy] : enemies)
		{
			if (enemy->IsDead()) continue;
			if (IsInArcXZ(player->GetPosition(), player->GetLook(),
				enemy->GetPosition(), reach, halfAngleDeg))
			{
				cout << "Player " << player->GetObjectId() << " hits Enemy " << enemy->GetObjectId()
					<< " (dmg=" << damage << " hp=" << enemy->GetCurrentHp() << ")" << endl;
				enemy->ApplyHit(tick.load(), damage, 20);
			}
		}
	}

	// 적 근접 공격 히트 판정
	for (auto& [eid, enemy] : enemies)
	{
		if (enemy->IsDead()) continue;
		if (enemy->GetAnimState() != Protocol::ANIMATION_TYPE_ATTACK) continue;

		const int elapsed = static_cast<int>(tick.load()) - enemy->GetAnimTick();
		constexpr int kHitFrameStart = 5;
		constexpr int kHitFrameEnd = 15;
		if (elapsed < kHitFrameStart || elapsed > kHitFrameEnd) continue;

		const int damage = enemy->GetAttackPower();

		float reach, halfAngleDeg;
		switch (enemy->GetWeaponState())
		{
		case Protocol::WEAPON_TYPE_SWORD: reach = 2.0f; halfAngleDeg = 45.0f; break;
		case Protocol::WEAPON_TYPE_AXE:   reach = 2.5f; halfAngleDeg = 45.0f; break;
		default:                          reach = 5.0f; halfAngleDeg = 90.0f; break;
		}

		for (auto& [pid, player] : players)
		{
			if (player->IsDead()) continue;
			if (IsInArcXZ(enemy->GetPosition(), enemy->GetLook(),
				player->GetPosition(), reach, halfAngleDeg))
			{
				player->ApplyHit(tick.load(), damage, 10);
			}
		}
	}

	if (_collision)
		_collision->OnUpdate();

	UpdateDynamicGridState();


	const auto elapsedMs = static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - frameStart).count());
	const uint64 nextDelayMs = (elapsedMs >= 160) ? 0 : (160 - elapsedMs);
	if (nextDelayMs == 0)
	{
		cout << "[TickAdvance] next tick immediately (elapsed=" << elapsedMs << "ms)" << endl;
		GRoom->DoAsync(&Room::TickAdvance);
	}
	else
	{
		GRoom->DoTimer(nextDelayMs, &Room::TickAdvance);
	}
	++tick;
}

ProjectileRef Room::AcquireFromPool(Vector<ProjectileRef>& pool)
{
	for (auto& p : pool)
	{
		if (!p->IsActive()) return p;
	}
	return nullptr;
}

void Room::FireArrow(PlayerRef shooter, float speed, uint32 lifeTicks)
{
	if (!shooter) return;
	if (shooter->IsDead()) return;
	if (shooter->GetWeaponState() != Protocol::WEAPON_TYPE_BOW) return;

	auto p = AcquireFromPool(m_arrowPool);
	if (!p) return;

	const GameMath::Vec3 origin = shooter->GetPosition() +
		shooter->GetRight() * 0.0626f +
		shooter->GetUp() * 1.5538f +
		shooter->GetLook() * 0.5657f;
	const GameMath::Vec3 forward = shooter->GetLook().Normalized();

	p->Activate(origin, forward * speed, lifeTicks, shooter->GetObjectId(), Protocol::BULLET_TYPE_ARROW);
	shooter->OnFired(tick.load());
}

void Room::FireCannonball(PlayerRef shooter)
{
	if (!shooter || !shooter->CanFire(tick.load())) return;
	if (shooter->IsDead()) return;

	auto p = AcquireFromPool(m_bulletPool);
	if (!p) return;

	const GameMath::Vec3 origin = shooter->GetPosition() +
		shooter->GetRight() * 0.1698f +
		shooter->GetUp() * 1.5665f +
		shooter->GetLook() * 0.2778f;
	const GameMath::Vec3 forward = shooter->GetLook().Normalized();
	constexpr float kBulletSpeed = 18.0f;
	constexpr int   kBulletLifeTicks = 100;

	p->Activate(origin, forward * kBulletSpeed, kBulletLifeTicks, shooter->GetObjectId(), Protocol::BULLET_TYPE_CANNONBALL);
	shooter->OnFired(tick.load());
	shooter->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
}