//-----------------------------------------------------------------------------
// File: EnemySpawner.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "EnemySpawner.h"
#include "Object.h"
#include "ColliderComponent.h"

void EnemySpawner::Initialize(const std::vector<EnemySpawnerPoolEntry>& spawnObjects)
{
	m_spawnEntries = spawnObjects;

	m_freeList.clear();
	m_freeList.reserve(m_spawnEntries.size());

	m_activeEnemies.clear();
	m_activeEnemies.reserve(m_spawnEntries.size());

	for ( size_t i = 0; i < m_spawnEntries.size(); ++i )
	{
		EnemySpawnerPoolEntry& entry = m_spawnEntries[i];

		CGameObject* enemy = entry.object;
		if ( !enemy )
			continue;

		enemy->SetActive(false);
		enemy->SetPosition(entry.spawnPosition);

		if ( auto* collider = enemy->GetComponent<CColliderComponent>() )
			collider->UpdateWorldBounds();

		m_freeList.push_back(i);
	}
}

CGameObject* EnemySpawner::ActivateEntry(
	size_t entryIndex,
	const DirectX::XMFLOAT3* overridePosition,
	const float* overrideYawDeg)
{
	if ( entryIndex >= m_spawnEntries.size() )
		return nullptr;

	EnemySpawnerPoolEntry& entry = m_spawnEntries[entryIndex];

	CGameObject* enemy = entry.object;
	if ( !enemy )
		return nullptr;

	const DirectX::XMFLOAT3 finalPosition =
		overridePosition ? *overridePosition : entry.spawnPosition;

	enemy->SetPosition(finalPosition);

	if ( overrideYawDeg )
	{
		if ( auto* tr = enemy->GetComponent<CTransformComponent>() )
		{
			tr->SetYawDegrees(*overrideYawDeg);
		}
		else
		{
			// CTransformComponent가 없을 경우 최소한 기존 yaw에서 delta 회전.
			// 일반 CGameObject에는 보통 TransformComponent가 있으므로 거의 타지 않는다.
			enemy->Rotate(0.0f, *overrideYawDeg, 0.0f);
		}
	}

	enemy->SetActive(true);

	if ( auto* collider = enemy->GetComponent<CColliderComponent>() )
	{
		collider->SetEnabled(true);
		collider->UpdateWorldBounds();
	}

	if ( std::find(m_activeEnemies.begin(), m_activeEnemies.end(), enemy) == m_activeEnemies.end() )
		m_activeEnemies.push_back(enemy);

	return enemy;
}

CGameObject* EnemySpawner::SpawnEnemy(
	int megaGridNumber,
	EEnemySpawnerEnemyKind kind)
{
	for ( size_t cursor = 0; cursor < m_freeList.size(); ++cursor )
	{
		const size_t entryIndex = m_freeList[cursor];

		if ( entryIndex >= m_spawnEntries.size() )
			continue;

		const EnemySpawnerPoolEntry& entry = m_spawnEntries[entryIndex];

		if ( entry.megaGridNumber != megaGridNumber )
			continue;

		if ( entry.kind != kind )
			continue;

		m_freeList[cursor] = m_freeList.back();
		m_freeList.pop_back();

		return ActivateEntry(entryIndex);
	}

	return nullptr;
}

CGameObject* EnemySpawner::SpawnEnemyAt(
	int megaGridNumber,
	EEnemySpawnerEnemyKind kind,
	const DirectX::XMFLOAT3& position,
	float yawDeg)
{
	for ( size_t cursor = 0; cursor < m_freeList.size(); ++cursor )
	{
		const size_t entryIndex = m_freeList[cursor];

		if ( entryIndex >= m_spawnEntries.size() )
			continue;

		const EnemySpawnerPoolEntry& entry = m_spawnEntries[entryIndex];

		if ( entry.megaGridNumber != megaGridNumber )
			continue;

		if ( entry.kind != kind )
			continue;

		m_freeList[cursor] = m_freeList.back();
		m_freeList.pop_back();

		return ActivateEntry(entryIndex, &position, &yawDeg);
	}

	return nullptr;
}

int EnemySpawner::SpawnEnemies(
	int megaGridNumber,
	EEnemySpawnerEnemyKind kind,
	int count)
{
	if ( count <= 0 )
		return 0;

	int spawnedCount = 0;

	for ( int i = 0; i < count; ++i )
	{
		if ( !SpawnEnemy(megaGridNumber, kind) )
			break;

		++spawnedCount;
	}

	return spawnedCount;
}

int EnemySpawner::SpawnMegaGrid(int megaGridNumber)
{
	int spawnedCount = 0;

	for ( size_t cursor = 0; cursor < m_freeList.size(); )
	{
		const size_t entryIndex = m_freeList[cursor];

		if ( entryIndex >= m_spawnEntries.size() )
		{
			m_freeList[cursor] = m_freeList.back();
			m_freeList.pop_back();
			continue;
		}

		const EnemySpawnerPoolEntry& entry = m_spawnEntries[entryIndex];

		if ( entry.megaGridNumber != megaGridNumber )
		{
			++cursor;
			continue;
		}

		m_freeList[cursor] = m_freeList.back();
		m_freeList.pop_back();

		if ( ActivateEntry(entryIndex) )
			++spawnedCount;
	}

	return spawnedCount;
}

void EnemySpawner::RemoveEnemy(CGameObject* enemy)
{
	if ( !enemy )
		return;

	for ( size_t i = 0; i < m_spawnEntries.size(); ++i )
	{
		if ( m_spawnEntries[i].object != enemy )
			continue;

		enemy->SetActive(false);

		auto activeIt = std::find(
			m_activeEnemies.begin(),
			m_activeEnemies.end(),
			enemy
		);

		if ( activeIt != m_activeEnemies.end() )
			m_activeEnemies.erase(activeIt);

		if ( std::find(m_freeList.begin(), m_freeList.end(), i) == m_freeList.end() )
			m_freeList.push_back(i);

		return;
	}
}

const std::vector<CGameObject*>& EnemySpawner::GetActiveEnemies() const
{
	return m_activeEnemies;
}

int EnemySpawner::GetActiveEnemyCount() const
{
	return static_cast< int >( m_activeEnemies.size() );
}