#include "pch.h"
#include "Room.h"
#include "Enemy.h"
#include "Player.h"
#include "BossScriptHost.h"
#include "BossAIContext.h"
#include "Projectile.h"
#include <lua/lua.hpp>
#include <cmath>

namespace
{
	constexpr uint32 kSpawnFxBossCallSummon = 1;
}

bool Room::IsPreBossMonster(uint64 enemyId) const
{
	if (enemyId == m_bossEnemyId) return false;
	if (m_bossSummonedEnemyIds.count(enemyId)) return false;
	if (m_poolEnemyMegaGrid.count(enemyId)) return false;

	auto it = enemies.find(enemyId);
	if (it == enemies.end() || !it->second) return false;

	return IsPositionInsideMegaGridNumber(it->second->GetPosition(), 5);
}

bool Room::AreAllPreBossMonstersDeadInMega5() const
{
	for (auto& [id, enemy] : enemies)
	{
		if (!enemy || !enemy->IsActive() || enemy->IsDead()) continue;
		if (IsPreBossMonster(id)) return false;
	}
	return true;
}

CEnemy* Room::GetBossEnemy()
{
	auto it = enemies.find(m_bossEnemyId);
	if (it == enemies.end() || !it->second) return nullptr;
	return it->second.get();
}

void Room::CallBossScriptUpdate(float dt)
{
	if (!m_bossScriptHost || !m_bossScriptHost->IsLoaded())
		return;

	if (m_bossAIContext)
	{
		m_bossAIContext->TickCooldowns(dt);
		m_bossAIContext->movedThisUpdate = false;
	}

	lua_State* L = m_bossScriptHost->GetState();
	lua_getglobal(L, "update");
	if (!lua_isfunction(L, -1))
	{
		lua_pop(L, 1);
		return;
	}

	lua_pushnumber(L, dt);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK)
	{
		cout << "[BossRoom] Script update error: " << lua_tostring(L, -1) << endl;
		lua_pop(L, 1);
	}

	if (m_bossAIContext && !m_bossAIContext->movedThisUpdate)
	{
		CEnemy* boss = GetBossEnemy();
		if (boss)
		{
			const Protocol::AnimationType anim = boss->GetAnimState();
			if (anim == Protocol::ANIMATION_TYPE_RUN ||
				anim == Protocol::ANIMATION_TYPE_WALK)
			{
				boss->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
				boss->SetLastMoveDir(GameMath::Vec3::Zero());
			}
		}
	}

	ProcessBossMeleeHit();
	ProcessBossSpellAction();
	ProcessBossCallAction();
	UpdateBossPoisonProjectiles(dt);
}

void Room::ProcessBossSpellAction()
{
	if (!m_bossAIContext) return;

	constexpr float kSpellProjectileDelay = 1.025f;

	float elapsed = m_bossAIContext->spellActionElapsed;
	if (elapsed < 0.0f || m_bossAIContext->spellProjectileSpawned) return;

	if (elapsed >= kSpellProjectileDelay)
	{
		SpawnBossPoisonProjectile();
		m_bossAIContext->spellProjectileSpawned = true;

		CEnemy* boss = GetBossEnemy();
		if (boss) boss->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
	}
}

void Room::SpawnBossPoisonProjectile()
{
	CEnemy* boss = GetBossEnemy();
	if (!boss) return;

	ProjectileRef slot;
	for (auto& p : m_bossPoisonPool)
	{
		if (!p->IsActive()) { slot = p; break; }
	}
	if (!slot) { cout << "[BossRoom] Poison pool exhausted" << endl; return; }

	constexpr float kSpawnOffsetY   = 3.3f;
	constexpr float kSpawnOffsetFwd = 4.0f;
	constexpr float kSpeed          = 18.0f;
	constexpr uint64 kLifetimeMs    = 15000;

	float yawRad = boss->GetYaw() * (3.14159265f / 180.0f);
	float fwdX = std::sin(yawRad);
	float fwdZ = std::cos(yawRad);

	auto bossPos = boss->GetPosition();
	GameMath::Vec3 spawnPos(
		bossPos.x + fwdX * kSpawnOffsetFwd,
		bossPos.y + kSpawnOffsetY,
		bossPos.z + fwdZ * kSpawnOffsetFwd);

	GameMath::Vec3 vel(fwdX * kSpeed, 0.0f, fwdZ * kSpeed);

	slot->Activate(spawnPos, vel, 1, kLifetimeMs, boss->GetObjectId(), Protocol::BULLET_TYPE_BOSS_POISON);
	m_bossPoisonHitMap[slot->GetObjectId()].clear();

	cout << "[BossRoom] Poison projectile spawned id=" << slot->GetObjectId() << endl;
}

void Room::UpdateBossPoisonProjectiles(float dt)
{
	constexpr float kStageHalfExtent  = 110.0f;
	constexpr float kStageCenterX     = 0.0f;
	constexpr float kStageCenterZ     = 400.0f;
	constexpr float kHitRadiusXZ      = 4.65f;  // gas radius 4.0 + player collision radius 0.65
	constexpr float kHitToleranceY    = 5.15f;  // gas radius 4.0 + player half height 1.15
	constexpr float kPlayerHitCenterY = 1.0f;
	constexpr int   kPoisonDamage     = 50;

	const uint32 animTick = GetAnimClockTick();
	const uint64 dtMs = static_cast<uint64>(dt * 1000.0f);

	for (auto& proj : m_bossPoisonPool)
	{
		if (!proj->IsActive()) continue;

		proj->Update(dt, dtMs);
		if (!proj->IsActive()) { m_bossPoisonHitMap.erase(proj->GetObjectId()); continue; }

		auto pos = proj->GetPosition();

		// Stage bounds check
		if (std::abs(pos.x - kStageCenterX) > kStageHalfExtent ||
			std::abs(pos.z - kStageCenterZ) > kStageHalfExtent)
		{
			proj->Deactivate();
			m_bossPoisonHitMap.erase(proj->GetObjectId());
			continue;
		}

		// Player hit check
		auto& hitSet = m_bossPoisonHitMap[proj->GetObjectId()];
		for (auto& [pid, player] : players)
		{
			if (!player || !player->IsActive() || player->IsDead()) continue;
			if (hitSet.count(pid)) continue;

			auto pPos = player->GetPosition();
			float dx = pPos.x - pos.x;
			float dz = pPos.z - pos.z;
			if (dx * dx + dz * dz > kHitRadiusXZ * kHitRadiusXZ) continue;
			if (std::abs((pPos.y + kPlayerHitCenterY) - pos.y) > kHitToleranceY) continue;

			player->ApplyHit(animTick, kPoisonDamage, 10, m_elapsedServerMs);
			hitSet.insert(pid);
			cout << "[BossRoom] Poison hit player " << pid << " for " << kPoisonDamage << endl;
		}
	}
}

void Room::ProcessBossMeleeHit()
{
	if (!m_bossAIContext) return;

	constexpr float kMeleeClipDuration = 1.5f;
	constexpr float kHitWindowStart    = 0.20f * kMeleeClipDuration;
	constexpr float kHitWindowEnd      = 0.55f * kMeleeClipDuration;
	constexpr float kMeleeRange        = 7.0f;
	constexpr float kMeleeArcCos       = 0.5f; // cos(60deg), ±60 degree arc
	constexpr int   kMeleeDamage       = 50;

	float elapsed = m_bossAIContext->meleeActionElapsed;
	if (elapsed < 0.0f) return;

	if (elapsed > kMeleeClipDuration)
	{
		m_bossAIContext->meleeActionElapsed = -1.0f;
		m_bossAIContext->meleeHitPlayerIds.clear();
		return;
	}

	if (elapsed < kHitWindowStart || elapsed > kHitWindowEnd) return;

	CEnemy* boss = GetBossEnemy();
	if (!boss) return;

	auto bossPos = boss->GetPosition();
	float yawRad = boss->GetYaw() * (3.14159265f / 180.0f);
	float fwdX = std::sin(yawRad);
	float fwdZ = std::cos(yawRad);

	const uint32 animTick = GetAnimClockTick();

	for (auto& [pid, player] : players)
	{
		if (!player || !player->IsActive() || player->IsDead()) continue;
		if (m_bossAIContext->meleeHitPlayerIds.count(pid)) continue;

		auto pPos = player->GetPosition();
		float dx = pPos.x - bossPos.x;
		float dz = pPos.z - bossPos.z;
		float distSq = dx * dx + dz * dz;
		if (distSq > kMeleeRange * kMeleeRange) continue;

		float dist = std::sqrt(distSq);
		if (dist > 0.001f)
		{
			float dot = (dx / dist) * fwdX + (dz / dist) * fwdZ;
			if (dot < kMeleeArcCos) continue;
		}

		player->ApplyHit(animTick, kMeleeDamage, 10, m_elapsedServerMs);
		m_bossAIContext->meleeHitPlayerIds.insert(pid);
		cout << "[BossRoom] Melee hit player " << pid << " for " << kMeleeDamage << endl;
	}
}

void Room::UpdateBossRoomState()
{
	if (m_bossRoomState == EBossRoomState::SummonFadeIn)
	{
		constexpr uint64 kSummonFadeInDurationMs = 3000;
		if (m_elapsedServerMs - m_bossRoomStateChangedMs >= kSummonFadeInDurationMs)
		{
			ActivateBoss();
			m_bossRoomState = EBossRoomState::BossAppearing;
			m_bossRoomStateChangedMs = m_elapsedServerMs;
			cout << "[BossRoom] BossAppearing" << endl;
		}
	}
	else if (m_bossRoomState == EBossRoomState::BossAppearing)
	{
		m_bossRoomState = EBossRoomState::BossActive;
		cout << "[BossRoom] BossActive" << endl;

		m_bossAIContext = std::make_unique<CBossAIContext>();
		m_bossAIContext->room = this;
		m_bossAIContext->bossEnemyId = m_bossEnemyId;

		m_bossScriptHost = std::make_unique<CBossScriptHost>();
		m_bossScriptHost->RegisterBossAPI(m_bossAIContext.get());

		if (!m_bossScriptHost->LoadScript("Scripts/Boss/BossStageBoss.lua"))
		{
			cout << "[BossRoom] Script load failed, Boss AI disabled" << endl;
			m_bossScriptHost.reset();
			m_bossAIContext.reset();
		}
	}
	else if (m_bossRoomState == EBossRoomState::BossDead)
	{
		constexpr uint64 kBossDeadDurationMs = 5000;
		if (m_elapsedServerMs - m_bossRoomStateChangedMs >= kBossDeadDurationMs)
		{
			m_bossRoomState = EBossRoomState::Cleared;
			cout << "[BossRoom] Cleared" << endl;
		}
	}
}

void Room::ActivateBoss()
{
	auto it = enemies.find(m_bossEnemyId);
	if (it == enemies.end() || !it->second)
	{
		cout << "[BossRoom] ActivateBoss: boss not found" << endl;
		return;
	}

	auto& boss = it->second;
	boss->SetPosition(m_bossOriginalPos);
	boss->SetYaw(m_bossOriginalYaw);
	boss->ResetHpToMax();
	boss->SetActive(true);
	SetObjectCollisionMegaGridMask(boss, ComputeMegaGridMaskFromWorldPosition(m_bossOriginalPos), true);

	cout << "[BossRoom] Boss activated at ("
		<< m_bossOriginalPos.x << ", " << m_bossOriginalPos.y << ", " << m_bossOriginalPos.z
		<< ")" << endl;
}

void Room::ProcessBossCallAction()
{
	if (!m_bossAIContext) return;

	constexpr float kRiseDuration    = 1.5f;
	constexpr float kRiseHeight      = 3.0f;
	constexpr float kCallClipDuration= 2.0f;
	constexpr float kSummonDelay     = 1.0f;
	constexpr float kDescendDuration = 2.0f;

	CEnemy* boss = GetBossEnemy();
	if (!boss) return;

	auto& ctx = *m_bossAIContext;

	if (ctx.callRiseElapsed >= 0.0f)
	{
		float t = (std::min)(ctx.callRiseElapsed / kRiseDuration, 1.0f);
		auto pos = boss->GetPosition();
		boss->SetPosition(pos.x, ctx.callStartY + kRiseHeight * t, pos.z);

		if (ctx.callRiseElapsed >= kRiseDuration)
		{
			boss->SetAnimState(Protocol::ANIMATION_TYPE_BOSS_CALL);
			ctx.callRiseElapsed  = -1.0f;
			ctx.callActionElapsed = 0.0f;
			cout << "[BossRoom] Call wave " << ctx.callExecutedCount << " rising complete" << endl;
		}
		return;
	}

	if (ctx.callActionElapsed >= 0.0f)
	{
		if (!ctx.callSummonDone && ctx.callActionElapsed >= kSummonDelay)
		{
			SpawnBossCallWave();
			ctx.callSummonDone = true;
		}

		if (ctx.callActionElapsed >= kCallClipDuration)
		{
			boss->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
			ctx.callActionElapsed  = -1.0f;
			ctx.callDescendElapsed = 0.0f;
		}
		return;
	}

	if (ctx.callDescendElapsed >= 0.0f)
	{
		float t = (std::min)(ctx.callDescendElapsed / kDescendDuration, 1.0f);
		auto pos = boss->GetPosition();
		boss->SetPosition(pos.x, ctx.callStartY + (kRiseHeight * (1.0f - t)), pos.z);

		if (ctx.callDescendElapsed >= kDescendDuration)
		{
			boss->SetPosition(pos.x, ctx.callStartY, pos.z);
			ctx.callDescendElapsed = -1.0f;
			cout << "[BossRoom] Call sequence complete" << endl;
		}
	}
}

void Room::SpawnBossCallWave()
{
	if (!m_bossAIContext) return;

	const int waveIndex = m_bossAIContext->callExecutedCount - 1;

	struct SpawnReq { Protocol::EnemyType type; int count; };
	std::vector<SpawnReq> wave;

	if (waveIndex == 0)
		wave = { { Protocol::ENEMY_TYPE_BASIC, 30 } };
	else if (waveIndex == 1)
		wave = { { Protocol::ENEMY_TYPE_BASIC, 20 }, { Protocol::ENEMY_TYPE_ARCHER, 5 }, { Protocol::ENEMY_TYPE_WARRIOR, 5 } };
	else if (waveIndex == 2)
		wave = { { Protocol::ENEMY_TYPE_BASIC, 20 }, { Protocol::ENEMY_TYPE_ARCHER, 5 }, { Protocol::ENEMY_TYPE_WARRIOR, 5 }, { Protocol::ENEMY_TYPE_MUTANT, 5 } };
	else
		return;

	int spawned = 0;
	for (auto& req : wave)
		for (int i = 0; i < req.count; i++)
			if (SpawnBossCallEnemy(req.type)) spawned++;

	cout << "[BossRoom] Call wave " << (waveIndex + 1) << " spawned " << spawned << " enemies" << endl;
}

CEnemy* Room::SpawnBossCallEnemy(Protocol::EnemyType type)
{
	if (!m_bossAIContext) return nullptr;

	float x   = m_bossAIContext->RandFloat(-100.0f,  100.0f);
	float z   = m_bossAIContext->RandFloat( 300.0f,  500.0f);
	float yaw = m_bossAIContext->RandFloat(-180.0f,  180.0f);

	CEnemy* activated = ActivateSpawnerEnemy(5, type, GameMath::Vec3(x, 0.0f, z), yaw);
	if (activated)
	{
		m_bossSummonedEnemyIds.insert(activated->GetObjectId());
		uint32 serial = ++m_spawnFxSerial;
		if (serial == 0)
			serial = ++m_spawnFxSerial;
		activated->SetSpawnFx(kSpawnFxBossCallSummon, GetTick(), serial);
		if (CMonsterAI* ai = activated->GetMonsterAI())
			ai->SetInfiniteDirectChaseMode();
	}

	return activated;
}

void Room::DebugDamageBoss()
{
	if (m_bossRoomState != EBossRoomState::BossActive) return;

	CEnemy* boss = GetBossEnemy();
	if (!boss) return;

	constexpr int kDebugDamage = 1200; // 25% of max HP 4800
	boss->ApplyHit(GetAnimClockTick(), kDebugDamage);
	cout << "[BossRoom] Debug: Boss HP " << boss->GetCurrentHp() << "/" << boss->GetMaxHp() << endl;
}
