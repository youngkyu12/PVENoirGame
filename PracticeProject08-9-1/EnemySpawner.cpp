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

CGameObject* EnemySpawner::SpawnEnemy(const DirectX::XMFLOAT3& position)
{
	if ( !mEnemyPool )
		return nullptr;

	CGameObject* enemy = mEnemyPool->Acquire();
	if ( !enemy )
		return nullptr;

	enemy->SetPosition(position);
	enemy->SetActive(true);

	mActiveEnemies.push_back(enemy);
	return enemy;
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