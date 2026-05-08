#include "stdafx.h"
#include "EnemySpawner.h"
#include "Object.h"

void EnemySpawner::Initialize(const std::vector<CGameObject*>& spawnObjects)
{
	m_SpawnObjects = spawnObjects;

	m_freeList.clear();
	m_freeList.reserve(m_SpawnObjects.size());

	for ( size_t i = 0; i < m_SpawnObjects.size(); ++i )
	{
		CGameObject* enemy = m_SpawnObjects[i];

		if ( !enemy )
			continue;

		enemy->SetActive(false);
		m_freeList.push_back(i);
	}
}

void EnemySpawner::Update(float deltaTime, const DirectX::XMFLOAT3& position)
{
	if ( deltaTime > 0.0f )
		mElapsedTime += deltaTime;

	if ( mElapsedTime > 10.0f ) {
		SetSpawnerPosition(position);
		SpawnEnemy();
	}

}

void EnemySpawner::SetSpawnerPosition(const DirectX::XMFLOAT3& position)
{
	mSpawnerPosition = position;
}

const DirectX::XMFLOAT3& EnemySpawner::GetSpawnerPosition() const
{
	return mSpawnerPosition;
}

CGameObject* EnemySpawner::SpawnEnemy()
{
	if ( m_freeList.empty() )
		return nullptr;

	size_t index = m_freeList.back();
	m_freeList.pop_back();

	CGameObject* enemy = m_SpawnObjects[index];

	if ( !enemy )
		return nullptr;

	enemy->SetActive(true);

	return enemy;
}

int EnemySpawner::SpawnEnemies(int count)
{
	if ( count <= 0 )
		return 0;

	int spawnedCount = 0;
	for ( int i = 0; i < count; ++i )
	{
		if ( !SpawnEnemy() )
			break;

		++spawnedCount;
	}

	return spawnedCount;
}

void EnemySpawner::RemoveEnemy(CGameObject* enemy)
{
	if ( !enemy )
		return;

	auto it = std::find(m_SpawnObjects.begin(), m_SpawnObjects.end(), enemy);
	if ( it == m_SpawnObjects.end() )
		return;
	(*it)->SetActive(false);
}

const std::vector<CGameObject*>& EnemySpawner::GetActiveEnemies() const
{
	return m_SpawnObjects;
}

int EnemySpawner::GetActiveEnemyCount() const
{
	return static_cast< int >( m_SpawnObjects.size() );
}