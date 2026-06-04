#include "pch.h"
#include "Room.h"
#include "Enemy.h"

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
