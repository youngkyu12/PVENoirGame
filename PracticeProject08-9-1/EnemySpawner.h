#pragma once

class CGameObject;
class CCamera;

class EnemySpawner
{
public:
	EnemySpawner() = default;
	~EnemySpawner() = default;

public:
	void Initialize(const std::vector<CGameObject*>& spawnObjects);
	void Update(float deltaTime);
	void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera);

public:
	void SetSpawnerPosition(const DirectX::XMFLOAT3& position);
	const DirectX::XMFLOAT3& GetSpawnerPosition() const;
	bool SpawnEnemy();
	int SpawnEnemies(int count = 30);
	void RemoveEnemy(CGameObject* enemy);

public:
	const std::vector<CGameObject*>& GetActiveEnemies() const;
	int GetActiveEnemyCount() const;

private:
	DirectX::XMFLOAT3 mSpawnerPosition = DirectX::XMFLOAT3(0.f, 0.f, 0.f);

	std::vector<CGameObject*> mActiveEnemies;
};