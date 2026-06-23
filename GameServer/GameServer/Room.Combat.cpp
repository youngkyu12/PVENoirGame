#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "Enemy.h"
#include "ColliderComponent.h"
#include "Projectile.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <vector>

namespace
{
	// 플레이어 무기 피해량
	constexpr int kPlayerWeaponDamageTierCount = 3;
	constexpr int kPlayerWeaponDamageMaxTierIndex = kPlayerWeaponDamageTierCount - 1;

	constexpr std::array<int, kPlayerWeaponDamageTierCount> kAtkPlayerSwordByTier =
	{
		10, 20, 40
	};

	constexpr std::array<int, kPlayerWeaponDamageTierCount> kAtkPlayerAxeByTier =
	{
		15, 30, 50
	};

	constexpr std::array<int, kPlayerWeaponDamageTierCount> kAtkPlayerArrowByTier =
	{
		15, 30, 50
	};

	constexpr std::array<int, kPlayerWeaponDamageTierCount> kAtkPlayerBulletByTier =
	{
		8, 18, 35
	};

	// 투사체 히트 판정
	constexpr float kProjectileHitRadiusSq = 1.0f;
	constexpr float kProjectileHitYTol     = 1.0f;

	// 근접 공격 유효 프레임 (애님 클락 기준)
	constexpr int kMeleeHitFrameStart = 5;
	constexpr int kMeleeHitFrameEnd   = 15;

	struct EnemyMeleeHitWindow
	{
		int startTick = 0;
		int endTick = 0;
		float reach = 0.0f;
		float halfAngleDeg = 0.0f;
		float yawOffsetDeg = 0.0f;
	};

	constexpr std::array<EnemyMeleeHitWindow, 3> kEnemySwordHitWindows =
	{ {
		{ 6, 7, 2.0f, 18.0f,  24.0f },
		{ 8, 9, 2.7f, 24.0f,   0.0f },
		{ 10, 10, 2.2f, 18.0f, -24.0f }
	} };

	constexpr std::array<EnemyMeleeHitWindow, 3> kEnemyAxeHitWindows =
	{ {
		{ 7, 8, 2.3f, 20.0f,  18.0f },
		{ 9, 10, 3.0f, 28.0f,  0.0f },
		{ 11, 11, 2.4f, 20.0f, -18.0f }
	} };

	constexpr std::array<EnemyMeleeHitWindow, 2> kEnemyDefaultHitWindows =
	{ {
		{ 6, 7, 1.5f, 22.0f,  12.0f },
		{ 8, 8, 1.9f, 28.0f, -8.0f }
	} };

	// 근접 공격 사거리 / 판정 각도
	constexpr float kMeleeReachSword         = 2.0f;
	constexpr float kMeleeReachAxe           = 2.5f;
	constexpr float kMeleeReachPlayerDefault = 1.5f;
	constexpr float kMeleeHalfAngleSword     = 45.0f;
	constexpr float kMeleeHalfAngleDefault   = 90.0f;

	// 포탄
	constexpr float kBulletSpeed     = 18.0f;
	constexpr int   kBulletLifeTicks = 100;

	template <size_t N>
	bool TryFindEnemyMeleeHitWindow(
		const std::array<EnemyMeleeHitWindow, N>& windows,
		int elapsedTick,
		EnemyMeleeHitWindow& outWindow)
	{
		for (const EnemyMeleeHitWindow& window : windows)
		{
			if (elapsedTick >= window.startTick &&
				elapsedTick <= window.endTick)
			{
				outWindow = window;
				return true;
			}
		}

		return false;
	}

	bool TryGetEnemyMeleeHitWindow(
		Protocol::WeaponType weapon,
		int elapsedTick,
		EnemyMeleeHitWindow& outWindow)
	{
		switch (weapon)
		{
		case Protocol::WEAPON_TYPE_SWORD:
			return TryFindEnemyMeleeHitWindow(kEnemySwordHitWindows, elapsedTick, outWindow);
		case Protocol::WEAPON_TYPE_AXE:
			return TryFindEnemyMeleeHitWindow(kEnemyAxeHitWindows, elapsedTick, outWindow);
		default:
			return TryFindEnemyMeleeHitWindow(kEnemyDefaultHitWindows, elapsedTick, outWindow);
		}
	}

	uint64 MakeMeleeHitKey(uint64 attackerId, uint64 targetId, uint32 attackAnimTick)
	{
		return ((attackerId & 0xFFFFFu) << 44) |
			((targetId & 0xFFFFFu) << 24) |
			(static_cast<uint64>(attackAnimTick) & 0xFFFFFFu);
	}

	bool IsInArcXZ(
		const GameMath::Vec3& attackerPos,
		const GameMath::Vec3& attackerLook,
		const GameMath::Vec3& targetPos,
		float reach,
		float halfAngleDeg,
		float yawOffsetDeg = 0.0f)
	{
		const float dx = targetPos.x - attackerPos.x;
		const float dz = targetPos.z - attackerPos.z;
		const float distSq = dx * dx + dz * dz;

		if (distSq > reach * reach) return false;
		if (distSq < 1e-8f) return true;

		const float dist = sqrtf(distSq);
		const float yawOffsetRad = yawOffsetDeg * GameMath::DEG_TO_RAD;
		const float cosOffset = cosf(yawOffsetRad);
		const float sinOffset = sinf(yawOffsetRad);
		const float arcLookX = attackerLook.x * cosOffset + attackerLook.z * sinOffset;
		const float arcLookZ = -attackerLook.x * sinOffset + attackerLook.z * cosOffset;
		const float dot = (dx / dist) * arcLookX + (dz / dist) * arcLookZ;
		const float cosHalf = cosf(halfAngleDeg * GameMath::DEG_TO_RAD);

		return dot >= cosHalf;
	}
}

int Room::ComputePlayerWeaponDamageTierIndex() const
{
	int clearedCount = 0;

	for (const MegaGridCell& cell : m_megaGridCells)
	{
		if (cell.isCleared)
			++clearedCount;
	}

	return std::clamp(clearedCount, 0, kPlayerWeaponDamageMaxTierIndex);
}

int Room::GetPlayerAttackPower(Protocol::WeaponType weapon) const
{
	const int tier = ComputePlayerWeaponDamageTierIndex();
	const size_t index = static_cast<size_t>(tier);

	switch (weapon)
	{
	case Protocol::WEAPON_TYPE_SWORD: return kAtkPlayerSwordByTier[index];
	case Protocol::WEAPON_TYPE_AXE:   return kAtkPlayerAxeByTier[index];
	case Protocol::WEAPON_TYPE_BOW:   return kAtkPlayerArrowByTier[index];
	case Protocol::WEAPON_TYPE_CANON: return kAtkPlayerBulletByTier[index];
	default: return 5;
	}
}

int Room::GetPlayerAttackPower(const PlayerRef& player, Protocol::WeaponType weapon) const
{
	const int baseDamage = GetPlayerAttackPower(weapon);
	return player ? player->ApplyAttackBuffToDamage(baseDamage, m_elapsedServerMs) : baseDamage;
}

bool Room::IsEnemyNearAnyPlayerExact(const GameMath::Vec3& enemyPos, float rangeSq) const
{
	for (const auto& playerPair : players)
	{
		const PlayerRef& player = playerPair.second;
		if (!player) continue;
		if (player->IsDead()) continue;
		if (GameMath::DistSqXZ(player->GetPosition(), enemyPos) <= rangeSq)
			return true;
	}

	return false;
}

void Room::WakeEnemiesNearPlayer(const PlayerRef& player)
{
	if (!player) return;
	if (player->IsDead()) return;

	const float wakeRange = m_timing.enemyAiWakeRange;
	const float wakeRangeSq = wakeRange * wakeRange;

	std::vector<uint64> candidateEnemyIds;
	CollectEnemyIdsInMegaGridRadius(player->GetPosition(), wakeRange, candidateEnemyIds);

	for (uint64 enemyId : candidateEnemyIds)
	{
		auto enemyIt = enemies.find(enemyId);
		if (enemyIt == enemies.end()) continue;

		const EnemyRef& enemy = enemyIt->second;
		if (!enemy) continue;
		if (!enemy->IsActive()) continue;
		if (enemy->IsDead()) continue;
		if (enemy->type == Protocol::ENEMY_TYPE_BOSS) continue;
		if (GameMath::DistSqXZ(player->GetPosition(), enemy->GetPosition()) > wakeRangeSq) continue;

		m_aiAwakeEnemyIds.insert(enemyId);
	}
}

void Room::ProcessEnemyAI()
{
	const auto frameStart = std::chrono::steady_clock::now();

	const float fixedDtSec = m_timing.enemyAiDtSec;
	const float sleepRange = m_timing.enemyAiSleepRange;
	const float sleepRangeSq = sleepRange * sleepRange;
	const uint32 animClockTick = GetAnimClockTick();

	for (auto it = m_aiAwakeEnemyIds.begin(); it != m_aiAwakeEnemyIds.end();)
	{
		const uint64 enemyId = *it;
		auto enemyIt = enemies.find(enemyId);
		if (enemyIt == enemies.end() || !enemyIt->second || !enemyIt->second->IsActive() || enemyIt->second->IsDead())
		{
			it = m_aiAwakeEnemyIds.erase(it);
			continue;
		}

		EnemyRef& enemy = enemyIt->second;
		if (enemy->type == Protocol::ENEMY_TYPE_BOSS)
		{
			it = m_aiAwakeEnemyIds.erase(it);
			continue;
		}

		CMonsterAI* ai = enemy->GetMonsterAI();
		if (ai && ai->IsOutsideHomeMegaGrid())
		{
			ai->ResetToHome();

			enemy->SetVelocity(GameMath::Vec3::Zero());
			enemy->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
			enemy->SetAnimTick(animClockTick);

			it = m_aiAwakeEnemyIds.erase(it);
			continue;
		}

		if (!IsEnemyNearAnyPlayerExact(enemy->GetPosition(), sleepRangeSq) &&
			(!ai || ai->IsAtHomeForAwakeRemoval()))
		{
			enemy->SetVelocity(GameMath::Vec3::Zero());
			enemy->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
			enemy->SetAnimTick(animClockTick);

			it = m_aiAwakeEnemyIds.erase(it);
			continue;
		}

		enemy->UpdateAI(fixedDtSec);
		enemy->SetPosition(SnapToTerrainIfBelow(enemy->GetPosition()));
		++it;
	}

	const auto elapsedMs = static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - frameStart).count());
	const uint64 enemyAiIntervalMs = m_timing.enemyAiIntervalMs;
	const uint64 nextDelayMs = (elapsedMs >= enemyAiIntervalMs) ? 0 : (enemyAiIntervalMs - elapsedMs);
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
	const uint32 animClockTick = GetAnimClockTick();
	const uint32 combatClockTick = GetCombatClockTick();

	if (m_meleeHitKeys.size() > 4096)
		m_meleeHitKeys.clear();

	TickDoorPortalCooldowns();

	for (auto player : players)
	{
		if (!player.second) continue;

		GameMath::Vec3 portalDestination = GameMath::Vec3::Zero();
		float portalYaw = 0.0f;
		float forcedYawDelta = 0.0f;
		int32 forcedTransformReason = 0;
		bool suppressTerrainSnap = false;
		if (player.second->ConsumePendingPortalTeleport(
			portalDestination,
			portalYaw,
			&forcedYawDelta,
			&forcedTransformReason,
			&suppressTerrainSnap))
		{
			player.second->SetVelocity(GameMath::Vec3::Zero());
			player.second->ClearMoveKeyCodes();
			player.second->SetTerrainSnapSuppressed(suppressTerrainSnap);
			player.second->SetPosition(portalDestination);
			player.second->SetYaw(portalYaw);
			if (forcedTransformReason != Protocol::FORCED_TRANSFORM_REASON_NONE)
				SendForcedTransformYawDelta(player.second, forcedYawDelta, forcedTransformReason);

			if (auto* collider = player.second->GetComponent<CColliderComponent>())
				collider->OnUpdate(0.0f);

			UpdateDynamicGridState();
			WakeEnemiesNearPlayer(player.second);
			continue;
		}

		const GameMath::Vec3 prevPos = player.second->GetPosition();
		player.second->Update(animClockTick);

		int32 respawnForcedTransformReason = 0;
		float respawnForcedYawDelta = 0.0f;
		if (player.second->ConsumeForcedTransformYawDelta(respawnForcedYawDelta, respawnForcedTransformReason) &&
			respawnForcedTransformReason != Protocol::FORCED_TRANSFORM_REASON_NONE)
		{
			SendForcedTransformYawDelta(player.second, respawnForcedYawDelta, respawnForcedTransformReason);
		}

		const bool teleported =
			TryTeleportPlayerByTowerDoorPortal(player.second) ||
			TryTeleportPlayerByCastleDoorPortal(player.second);

		if (!teleported)
		{
			ResolveWorldStaticCollision(player.second, prevPos);
			if (!player.second->IsTerrainSnapSuppressed())
				player.second->SetPosition(SnapToTerrainIfBelow(player.second->GetPosition()));
		}

		WakeEnemiesNearPlayer(player.second);
	}

	RefreshDynamicCollisionMegaGridMasks();

	for (auto& [pid, player] : players)
	{
		if (!player) continue;
		if (player->IsDead()) continue;

		WeaponFireRequest req = player->GetWeapon().UpdateAttack(combatClockTick);
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
		enemy.second->Update(animClockTick);
	}

	// 화살 히트
	for (auto& p : m_arrowPool)
	{
		if (!p->IsActive()) continue;
		p->Update(m_timing.projectileDtSec, m_timing.serverTickIntervalMs);
		const uint16_t projectileMask = ComputeObjectCurrentMegaGridMask(p.get());

		for (auto& enemyPair : enemies)
		{
			auto& enemy = enemyPair.second;
			if (enemy->IsDead()) continue;
			uint16_t enemyMask = 0;
			if (auto* enemyCollider = enemy->GetComponent<CColliderComponent>())
				enemyMask = enemyCollider->GetCollisionMegaGridMask();
			if (enemyMask == 0)
				enemyMask = ComputeObjectCurrentMegaGridMask(enemy.get());
			if (projectileMask != 0 && enemyMask != 0 && (projectileMask & enemyMask) == 0) continue;

			const float distSq = GameMath::DistSqXZ(enemy->GetPosition(), p->GetPosition());
			const bool hit = distSq <= kProjectileHitRadiusSq
				&& enemy->GetPosition().y - p->GetPosition().y <= kProjectileHitYTol;
			if (!hit) continue;

			enemy->ApplyHit(animClockTick, p->GetAttackPower(), 20);
			p->Deactivate();
			break;
		}
	}

	// 포탄 히트
	for (auto& p : m_bulletPool)
	{
		if (!p->IsActive()) continue;
		p->Update(m_timing.projectileDtSec, m_timing.serverTickIntervalMs);
		const uint16_t projectileMask = ComputeObjectCurrentMegaGridMask(p.get());

		for (auto& enemyPair : enemies)
		{
			auto& enemy = enemyPair.second;
			if (enemy->IsDead()) continue;
			uint16_t enemyMask = 0;
			if (auto* enemyCollider = enemy->GetComponent<CColliderComponent>())
				enemyMask = enemyCollider->GetCollisionMegaGridMask();
			if (enemyMask == 0)
				enemyMask = ComputeObjectCurrentMegaGridMask(enemy.get());
			if (projectileMask != 0 && enemyMask != 0 && (projectileMask & enemyMask) == 0) continue;

			const float distSq = GameMath::DistSqXZ(enemy->GetPosition(), p->GetPosition());
			const bool hit = distSq <= kProjectileHitRadiusSq
				&& enemy->GetPosition().y - p->GetPosition().y <= kProjectileHitYTol;
			if (!hit) continue;

			enemy->ApplyHit(animClockTick, p->GetAttackPower(), 20);
			p->Deactivate();
			break;
		}
	}

	// 적 화살 히트
	constexpr int kAtkEnemyArrow = 10;

	for (auto& p : m_enemyArrowPool)
	{
		if (!p->IsActive()) continue;
		p->Update(m_timing.projectileDtSec, m_timing.serverTickIntervalMs);

		for (auto& [pid, player] : players)
		{
			if (!player || player->IsDead()) continue;

			const float distSq = GameMath::DistSqXZ(player->GetPosition(), p->GetPosition());
			const float yDiff  = std::abs(player->GetPosition().y - p->GetPosition().y);
			if (distSq > kProjectileHitRadiusSq || yDiff > kProjectileHitYTol) continue;

			player->ApplyHit(animClockTick, kAtkEnemyArrow, 10, m_elapsedServerMs);
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

		const int elapsed = static_cast<int>(animClockTick) - player->GetAnimTick();
		if (elapsed < kMeleeHitFrameStart || elapsed > kMeleeHitFrameEnd) continue;

		const int damage = GetPlayerAttackPower(player, weaponType);

		float reach, halfAngleDeg;
		switch (weaponType)
		{
		case Protocol::WEAPON_TYPE_SWORD: reach = kMeleeReachSword; halfAngleDeg = kMeleeHalfAngleSword;   break;
		case Protocol::WEAPON_TYPE_AXE:   reach = kMeleeReachAxe;   halfAngleDeg = kMeleeHalfAngleSword;   break;
		default:                          reach = kMeleeReachPlayerDefault; halfAngleDeg = kMeleeHalfAngleDefault; break;
		}

		for (auto& [eid, enemy] : enemies)
		{
			if (enemy->IsDead()) continue;
			const uint16_t playerMask = ComputeObjectCurrentMegaGridMask(player.get());
			uint16_t enemyMask = 0;
			if (auto* enemyCollider = enemy->GetComponent<CColliderComponent>())
				enemyMask = enemyCollider->GetCollisionMegaGridMask();
			if (enemyMask == 0)
				enemyMask = ComputeObjectCurrentMegaGridMask(enemy.get());
			if (playerMask != 0 && enemyMask != 0 && (playerMask & enemyMask) == 0) continue;
			if (IsInArcXZ(player->GetPosition(), player->GetLook(),
				enemy->GetPosition(), reach, halfAngleDeg))
			{
				const uint64 hitKey = MakeMeleeHitKey(
					player->GetObjectId(),
					enemy->GetObjectId(),
					player->GetAnimTick());
				if (!m_meleeHitKeys.insert(hitKey).second)
					continue;

				cout << "Player " << player->GetObjectId() << " hits Enemy " << enemy->GetObjectId()
					<< " (dmg=" << damage << " hp=" << enemy->GetCurrentHp() << ")" << endl;
				enemy->ApplyHit(animClockTick, damage, 20);
			}
		}
	}

	// 적 근접 공격 히트 판정
	for (auto& [eid, enemy] : enemies)
	{
		if (enemy->IsDead()) continue;
		if (enemy->GetAnimState() != Protocol::ANIMATION_TYPE_ATTACK) continue;

		const int elapsed = static_cast<int>(animClockTick) - enemy->GetAnimTick();
		EnemyMeleeHitWindow hitWindow{};
		if (!TryGetEnemyMeleeHitWindow(enemy->GetWeaponState(), elapsed, hitWindow)) continue;

		const int damage = enemy->GetAttackPower();

		for (auto& [pid, player] : players)
		{
			if (player->IsDead()) continue;
			uint16_t enemyMask = 0;
			if (auto* enemyCollider = enemy->GetComponent<CColliderComponent>())
				enemyMask = enemyCollider->GetCollisionMegaGridMask();
			if (enemyMask == 0)
				enemyMask = ComputeObjectCurrentMegaGridMask(enemy.get());
			const uint16_t playerMask = ComputeObjectCurrentMegaGridMask(player.get());
			if (enemyMask != 0 && playerMask != 0 && (enemyMask & playerMask) == 0) continue;
			if (IsInArcXZ(enemy->GetPosition(), enemy->GetLook(),
				player->GetPosition(), hitWindow.reach, hitWindow.halfAngleDeg, hitWindow.yawOffsetDeg))
			{
				const uint64 hitKey = MakeMeleeHitKey(
					enemy->GetObjectId(),
					player->GetObjectId(),
					enemy->GetAnimTick());
				if (!m_meleeHitKeys.insert(hitKey).second)
					continue;

				player->ApplyHit(animClockTick, damage, 10, m_elapsedServerMs);
			}
		}
	}

	UpdateDynamicGridState();
	UpdateKeyPickupCollision();
	UpdateItemPickupCollision();
	UpdateSpawnerWaves(m_timing.playerInputDtSec);
	UpdateBossRoomState();
	if (m_bossRoomState == EBossRoomState::BossActive)
		CallBossScriptUpdate(m_timing.playerInputDtSec);

	const auto elapsedMs = static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - frameStart).count());
	const uint64 serverTickIntervalMs = m_timing.serverTickIntervalMs;
	const uint64 nextDelayMs = (elapsedMs >= serverTickIntervalMs) ? 0 : (serverTickIntervalMs - elapsedMs);
	if (nextDelayMs == 0)
	{
		cout << "[TickAdvance] next tick immediately (elapsed=" << elapsedMs << "ms)" << endl;
		GRoom->DoAsync(&Room::TickAdvance);
	}
	else
	{
		GRoom->DoTimer(nextDelayMs, &Room::TickAdvance);
	}
	m_elapsedServerMs += m_timing.serverTickIntervalMs;
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

void Room::UpdateKeyPickupCollision()
{
	constexpr float kPickupRadiusSq = 1.25f * 1.25f;
	constexpr float kPickupYTolerance = 2.0f;

	for (const auto& key : kKeyPositions)
	{
		const int megaGrid = key.megaGridIndex + 1;
		if (!m_keyPickupUnlockedByMegaGrid[static_cast<size_t>(megaGrid)])
			continue;

		MegaGridCell& cell = m_megaGridCells[static_cast<size_t>(key.megaGridIndex)];
		if (cell.isCleared)
			continue;

		for (auto& [pid, player] : players)
		{
			if (!player || player->IsDead())
				continue;

			const GameMath::Vec3 pos = player->GetPosition();
			if (pos.y < -100.0f)
				continue;

			const float dx = pos.x - key.x;
			const float dz = pos.z - key.z;
			if (dx * dx + dz * dz > kPickupRadiusSq)
				continue;

			cell.isCleared = true;
			for (auto& item : m_items)
			{
				if (item.kind != Protocol::ITEM_TYPE_KEY) continue;
				if (std::abs(item.position.x - key.x) < 0.1f &&
					std::abs(item.position.z - key.z) < 0.1f)
				{
					item.active = false;
					break;
				}
			}
			cout << "[Key Pickup] MegaGrid " << (key.megaGridIndex + 1)
				<< " cleared by Player " << player->GetObjectId() << endl;
			break;
		}
	}
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

	p->Activate(origin, forward * speed, lifeTicks, m_timing.projectileLifeTickMs, shooter->GetObjectId(), Protocol::BULLET_TYPE_ARROW);
	p->SetAttackPower(GetPlayerAttackPower(shooter, Protocol::WEAPON_TYPE_BOW));
	shooter->OnFired(GetCombatClockTick());
}

void Room::FireEnemyArrow(CServerObject* shooter, float speed, uint32 lifeTicks)
{
	if (!shooter || shooter->IsDead()) return;

	auto p = AcquireFromPool(m_enemyArrowPool);
	if (!p) return;

	const GameMath::Vec3 origin = shooter->GetPosition()
		+ shooter->GetUp()   * 1.5f
		+ shooter->GetLook() * 0.5f;

	const GameMath::Vec3 forward = shooter->GetLook().Normalized();
	const GameMath::Vec3 vel(forward.x * speed, 0.0f, forward.z * speed);

	p->Activate(origin, vel, lifeTicks, m_timing.projectileLifeTickMs,
				shooter->GetObjectId(), Protocol::BULLET_TYPE_ARROW);
	p->SetAttackPower(shooter->GetAttackPower());

	cout << "[FireEnemyArrow] shooter=" << shooter->GetObjectId()
		<< " pos=(" << origin.x << "," << origin.y << "," << origin.z << ")" << endl;
}

void Room::FireCannonball(PlayerRef shooter)
{
	if (!shooter || !shooter->CanFire(GetCombatClockTick())) return;
	if (shooter->IsDead()) return;

	auto p = AcquireFromPool(m_bulletPool);
	if (!p) return;

	const GameMath::Vec3 origin = shooter->GetPosition() +
		shooter->GetRight() * 0.1698f +
		shooter->GetUp() * 1.5665f +
		shooter->GetLook() * 0.2778f;
	const GameMath::Vec3 forward = shooter->GetLook().Normalized();

	p->Activate(origin, forward * kBulletSpeed, kBulletLifeTicks, m_timing.projectileLifeTickMs, shooter->GetObjectId(), Protocol::BULLET_TYPE_CANNONBALL);
	p->SetAttackPower(GetPlayerAttackPower(shooter, Protocol::WEAPON_TYPE_CANON));
	shooter->OnFired(GetCombatClockTick());
	shooter->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
}
