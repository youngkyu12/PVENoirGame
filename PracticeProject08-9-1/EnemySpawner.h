#pragma once

class CGameObject;
class CCamera;

class EnemySpawner
{
public:
	EnemySpawner() = default;
	virtual ~EnemySpawner() = default;

public:
	void Initialize(const std::vector<CGameObject*>& spawnObjects);
	void Update(float deltaTime, const DirectX::XMFLOAT3& position);

public:
	void SetSpawnerPosition(const DirectX::XMFLOAT3& position);
	const DirectX::XMFLOAT3& GetSpawnerPosition() const;
	CGameObject* SpawnEnemy();
	int SpawnEnemies(int count = 30);
	void RemoveEnemy(CGameObject* enemy);

public:
	const std::vector<CGameObject*>& GetActiveEnemies() const;
	int GetActiveEnemyCount() const;

private:
	DirectX::XMFLOAT3 mSpawnerPosition = DirectX::XMFLOAT3(0.f, 0.f, 0.f);
	float mElapsedTime = 0.0f;
	bool flag = false;

	std::vector<CGameObject*> m_SpawnObjects;
	std::vector<size_t> m_freeList;
};