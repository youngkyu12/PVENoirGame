#include "pch.h"
#include "Room.h"
#include "Enemy.h"
#include "BossScriptHost.h"
#include "BossAIContext.h"
#include <lua/lua.hpp>

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
		m_bossAIContext->TickCooldowns(dt);

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
