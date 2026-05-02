#pragma once

#include <vector>
#include <DirectXMath.h>

class CGameObject;
class EnemyPool;

class EnemySpawner
{
public:
	EnemySpawner() = default;
	~EnemySpawner() = default;

public:
	void Initialize(EnemyPool* enemyPool);
	void Update(float deltaTime);
	void Render();

public:
	CGameObject* SpawnEnemy(const DirectX::XMFLOAT3& position);
	void RemoveEnemy(CGameObject* enemy);
	void Clear();

public:
	const std::vector<CGameObject*>& GetActiveEnemies() const;
	int GetActiveEnemyCount() const;

private:
	EnemyPool* mEnemyPool = nullptr;

	std::vector<CGameObject*> mActiveEnemies;
};