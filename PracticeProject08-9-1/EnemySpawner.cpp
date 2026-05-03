#include "stdafx.h"
#include "EnemySpawner.h"
#include "Object.h"
#include "EnemyPool.h"

void EnemySpawner::Initialize(EnemyPool* enemyPool)
{
	mEnemyPool = enemyPool;
	mActiveEnemies.clear();
}

void EnemySpawner::Update(float deltaTime)
{
	UNREFERENCED_PARAMETER(deltaTime);
}

void EnemySpawner::Render()
{
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
	if ( !mEnemyPool )
		return nullptr;

	CGameObject* enemy = mEnemyPool->Acquire();
	if ( !enemy )
		return nullptr;

	enemy->SetPosition(mSpawnerPosition);
	enemy->SetActive(true);

	mActiveEnemies.push_back(enemy);
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

	auto it = std::find(mActiveEnemies.begin(), mActiveEnemies.end(), enemy);
	if ( it == mActiveEnemies.end() )
		return;

	if ( mEnemyPool )
		mEnemyPool->Release(enemy);

	mActiveEnemies.erase(it);
}

void EnemySpawner::Clear()
{
	if ( mEnemyPool )
	{
		for ( CGameObject* enemy : mActiveEnemies )
		{
			mEnemyPool->Release(enemy);
		}
	}

	mActiveEnemies.clear();
}

const std::vector<CGameObject*>& EnemySpawner::GetActiveEnemies() const
{
	return mActiveEnemies;
}

int EnemySpawner::GetActiveEnemyCount() const
{
	return static_cast< int >( mActiveEnemies.size() );
}