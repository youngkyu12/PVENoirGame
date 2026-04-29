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

	// 부채꼴 범위 내 타겟인지 판정
	// attacker → 공격자 위치/방향, target → 피격 대상 위치
	// reach: 사거리, halfAngleDeg: 부채꼴 반각 (90도 부채꼴이면 45도)

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

		// 1) 거리 체크
		if (distSq > reach * reach)
			return false;
		if (distSq < 1e-8f)
			return true; // 겹쳐있으면 히트

		// 2) 각도 체크
		const float dist = sqrtf(distSq);
		const float dirX = dx / dist;
		const float dirZ = dz / dist;

		// attackerLook은 이미 정규화되어있다고 가정 (GetLook())
		const float dot = dirX * attackerLook.x + dirZ * attackerLook.z;
		const float cosHalf = cosf(halfAngleDeg * GameMath::DEG_TO_RAD);

		return dot >= cosHalf;
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

	// ── 플레이어 근접 공격 히트 판정 ──
	for (auto& [pid, player] : players)
	{
		if (player->GetAnimState() != Protocol::ANIMATION_TYPE_ATTACK)
			continue;

		const auto weaponType = player->GetWeaponState();
		if (weaponType == Protocol::WEAPON_TYPE_BOW ||
			weaponType == Protocol::WEAPON_TYPE_CANON)
			continue; // 원거리는 발사체로 처리

		// 공격 시작 후 히트 프레임인지 확인 (예: 공격 시작 5~15틱 구간)
		const int elapsed = static_cast<int>(tick.load()) - player->GetAnimTick();
		constexpr int kHitFrameStart = 5;
		constexpr int kHitFrameEnd = 15;
		if (elapsed < kHitFrameStart || elapsed > kHitFrameEnd)
			continue;

		// 이미 이 공격에서 히트한 적은 스킵 (다중 히트 방지)
		// → player에 m_meleeHitSet 같은 걸 추가하거나, 
		//   간단하게 kHitFrameStart 틱에서만 1회 판정

		float reach, halfAngleDeg;
		switch (weaponType)
		{
		case Protocol::WEAPON_TYPE_SWORD: reach = 2.0f; halfAngleDeg = 45.0f; break;
		case Protocol::WEAPON_TYPE_AXE:   reach = 2.5f; halfAngleDeg = 45.0f; break;
		default:                          reach = 1.5f; halfAngleDeg = 90.0f; break;
		}

		for (auto& [eid, enemy] : enemies)
		{
			if (IsInArcXZ(
				player->GetPosition(), player->GetLook(),
				enemy->GetPosition(), reach, halfAngleDeg))
			{
				cout << "Player " << player->GetObjectId() << " hits Enemy " << enemy->GetObjectId() << endl;
				enemy->ApplyHit(tick.load(), 20);
			}
		}
	}

	// ── 적 근접 공격 히트 판정 ──
	for (auto& [eid, enemy] : enemies)
	{
		if (enemy->GetAnimState() != Protocol::ANIMATION_TYPE_ATTACK)
			continue;

		const int elapsed = static_cast<int>(tick.load()) - enemy->GetAnimTick();
		constexpr int kHitFrameStart = 5;
		constexpr int kHitFrameEnd = 15;
		if (elapsed < kHitFrameStart || elapsed > kHitFrameEnd)
			continue;

		// 적 무기에 따른 arc 설정
		float reach, halfAngleDeg;
		switch (enemy->GetWeaponState())
		{
		case Protocol::WEAPON_TYPE_SWORD: reach = 2.0f; halfAngleDeg = 45.0f; break;
		case Protocol::WEAPON_TYPE_AXE:   reach = 2.5f; halfAngleDeg = 45.0f; break;
		default:                          reach = 5.0f; halfAngleDeg = 90.0f; break; // 반원
		}

		for (auto& [pid, player] : players)
		{
			if (IsInArcXZ(
				enemy->GetPosition(), enemy->GetLook(),
				player->GetPosition(), reach, halfAngleDeg))
			{
				player->ApplyHit(tick.load(), 10);
			}
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
