#include "stdafx.h"
#include "EnemySpawner.h"
#include "Object.h"

void EnemySpawner::Initialize(const std::vector<CGameObject*>& spawnObjects)
{
	m_SpawnObjects = spawnObjects;

	mElapsedTime = 0.0f;
	flag = false;
	mSpawnerPosition = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

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

	if ( deltaTime > 0.0f && mElapsedTime < 15.0f)
		mElapsedTime += deltaTime;

	if ( mElapsedTime > 10.0f && !flag) {
		SetSpawnerPosition(position);
		SpawnEnemy();
		flag = true;
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
	enemy->SetPosition(mSpawnerPosition);
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
	
	const size_t index = static_cast< size_t >(
		std::distance(m_SpawnObjects.begin(), it)
	);

	enemy->SetActive(false);

	if ( std::find(m_freeList.begin(), m_freeList.end(), index) == m_freeList.end() )
	{
		m_freeList.push_back(index);
	}
}

const std::vector<CGameObject*>& EnemySpawner::GetActiveEnemies() const
{
	return m_SpawnObjects;
}

int EnemySpawner::GetActiveEnemyCount() const
{
	return static_cast< int >( m_SpawnObjects.size() );
}