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

		DirectX::XMFLOAT3 inactivePosition = entry.spawnPosition;
		inactivePosition.y = -100.0f;

		enemy->SetActive(false);
		enemy->SetPosition(inactivePosition);

		if ( auto* collider = enemy->GetComponent<CColliderComponent>() )
		{
			collider->SetEnabled(false);
			collider->UpdateWorldBounds();
		}

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

int EnemySpawner::PeekSpawnEntries(
	int megaGridNumber,
	EEnemySpawnerEnemyKind kind,
	int count,
	std::vector<EnemySpawnerPreviewEntry>& outEntries) const
{
	if ( count <= 0 )
		return 0;

	int foundCount = 0;

	for ( size_t cursor = 0;
		  cursor < m_freeList.size() && foundCount < count;
		  ++cursor )
	{
		const size_t entryIndex = m_freeList[cursor];

		if ( entryIndex >= m_spawnEntries.size() )
			continue;

		const EnemySpawnerPoolEntry& entry = m_spawnEntries[entryIndex];

		if ( entry.megaGridNumber != megaGridNumber )
			continue;

		if ( entry.kind != kind )
			continue;

		EnemySpawnerPreviewEntry preview{};
		preview.entryIndex = entryIndex;
		preview.object = entry.object;
		preview.kind = entry.kind;
		preview.megaGridNumber = entry.megaGridNumber;
		preview.spawnPosition = entry.spawnPosition;

		outEntries.push_back(preview);
		++foundCount;
	}

	return foundCount;
}

CGameObject* EnemySpawner::SpawnPreviewEntry(
	const EnemySpawnerPreviewEntry& preview)
{
	const size_t entryIndex = preview.entryIndex;

	if ( entryIndex >= m_spawnEntries.size() )
		return nullptr;

	EnemySpawnerPoolEntry& entry = m_spawnEntries[entryIndex];

	// preview 시점과 실제 spawn 시점의 엔트리가 같은지 검증.
	if ( entry.object != preview.object )
		return nullptr;

	if ( entry.kind != preview.kind )
		return nullptr;

	if ( entry.megaGridNumber != preview.megaGridNumber )
		return nullptr;

	for ( size_t cursor = 0; cursor < m_freeList.size(); ++cursor )
	{
		if ( m_freeList[cursor] != entryIndex )
			continue;

		m_freeList[cursor] = m_freeList.back();
		m_freeList.pop_back();

		return ActivateEntry(entryIndex);
	}

	// 이미 다른 경로에서 활성화됐거나 freeList에서 빠진 경우.
	return nullptr;
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

		DirectX::XMFLOAT3 inactivePosition =
			m_spawnEntries[i].spawnPosition;

		inactivePosition.y = -100.0f;

		enemy->SetActive(false);
		enemy->SetPosition(inactivePosition);

		if ( auto* collider = enemy->GetComponent<CColliderComponent>() )
		{
			collider->SetEnabled(false);
			collider->UpdateWorldBounds();
		}

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