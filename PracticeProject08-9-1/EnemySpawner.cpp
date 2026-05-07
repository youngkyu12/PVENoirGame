#include "stdafx.h"
#include "EnemySpawner.h"
#include "Object.h"

void EnemySpawner::Initialize(const std::vector<CGameObject*>& spawnObjects)
{
	mActiveEnemies = spawnObjects;
	for ( CGameObject* enemy : mActiveEnemies )
	{
		if ( enemy )
			enemy->SetActive(false);
	}
}

void EnemySpawner::Update(float deltaTime)
{
	if ( deltaTime > 0.0f )
		mElapsedTime += deltaTime;

	if ( mElapsedTime > 10.0f)
		SpawnEnemy();
}

void EnemySpawner::SetSpawnerPosition(const DirectX::XMFLOAT3& position)
{
	mSpawnerPosition = position;
}

const DirectX::XMFLOAT3& EnemySpawner::GetSpawnerPosition() const
{
	return mSpawnerPosition;
}

bool EnemySpawner::SpawnEnemy()
{
	CGameObject* enemy = mActiveEnemies.back();
	if ( enemy == nullptr )
		return false;

	enemy->SetPosition(mSpawnerPosition);
	enemy->SetActive(true);

	return true;
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
	(*it)->SetActive(false);
}

const std::vector<CGameObject*>& EnemySpawner::GetActiveEnemies() const
{
	return mActiveEnemies;
}

int EnemySpawner::GetActiveEnemyCount() const
{
	return static_cast< int >( mActiveEnemies.size() );
}