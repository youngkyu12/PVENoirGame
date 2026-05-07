#include "stdafx.h"
#include "EnemySpawner.h"
#include "Object.h"

void EnemySpawner::Initialize(const std::vector<CGameObject*>& spawnObjects)
{
	mActiveEnemies = spawnObjects;
	mActiveEnemies.clear();
	for ( CGameObject* enemy : mActiveEnemies )
	{
		if ( enemy )
			enemy->SetActive(false);
	}
}

void EnemySpawner::Update(float deltaTime)
{
	for ( CGameObject* enemy : mActiveEnemies )
	{
		if ( !enemy || !enemy->IsActive() )
			continue;

		enemy->Animate(deltaTime);
	}
}

void EnemySpawner::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd )
		return;

	int activeEnemyCount = 0;
	int rendererEnabledCount = 0;
	int rendererMissingCount = 0;

	for ( CGameObject* enemy : mActiveEnemies )
	{
		if ( !enemy || !enemy->IsActive() )
			continue;

		++activeEnemyCount;
		CRendererComponent* renderer = enemy->GetRenderer();
		if ( !renderer )
			++rendererMissingCount;
		else if ( renderer->IsEnabled() )
			++rendererEnabledCount;

		enemy->Render(cmd, camera);
	}

	static int sDebugFrameCounter = 0;
	++sDebugFrameCounter;
	if ( ( sDebugFrameCounter % 120 ) == 0 )
	{
		char dbg[256] = {};
		std::snprintf(
			dbg,
			sizeof(dbg),
			"[EnemySpawner::Render] active=%d renderer_enabled=%d renderer_missing=%d total_tracked=%zu\n",
			activeEnemyCount,
			rendererEnabledCount,
			rendererMissingCount,
			mActiveEnemies.size()
		);
		OutputDebugStringA(dbg);
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

bool EnemySpawner::SpawnEnemy()
{
	CGameObject* enemy = mActiveEnemies.back();
	mActiveEnemies.pop_back();
	if ( !enemy )
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