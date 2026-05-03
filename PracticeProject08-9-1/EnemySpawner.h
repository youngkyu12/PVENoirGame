#pragma once

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
	void SetSpawnerPosition(const DirectX::XMFLOAT3& position);
	const DirectX::XMFLOAT3& GetSpawnerPosition() const;
	CGameObject* SpawnEnemy();
	int SpawnEnemies(int count = 30);
	void RemoveEnemy(CGameObject* enemy);
	void Clear();

public:
	const std::vector<CGameObject*>& GetActiveEnemies() const;
	int GetActiveEnemyCount() const;

private:
	EnemyPool* mEnemyPool = nullptr;
	DirectX::XMFLOAT3 mSpawnerPosition = DirectX::XMFLOAT3(0.f, 0.f, 0.f);

	std::vector<CGameObject*> mActiveEnemies;
};