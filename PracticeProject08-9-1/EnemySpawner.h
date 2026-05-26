//-----------------------------------------------------------------------------
// File: EnemySpawner.h
//-----------------------------------------------------------------------------
#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <DirectXMath.h>

class CGameObject;

enum class EEnemySpawnerEnemyKind : uint8_t
{
	Ghoul = 0,
	SwordMan,
	BowMan,
	Mutant
};

struct EnemySpawnerPoolEntry
{
	CGameObject* object = nullptr;
	EEnemySpawnerEnemyKind kind = EEnemySpawnerEnemyKind::Ghoul;
	int megaGridNumber = -1;
	DirectX::XMFLOAT3 spawnPosition = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
};

struct EnemySpawnerPreviewEntry
{
	size_t entryIndex = static_cast< size_t >( -1 );

	CGameObject* object = nullptr;
	EEnemySpawnerEnemyKind kind = EEnemySpawnerEnemyKind::Ghoul;
	int megaGridNumber = -1;
	DirectX::XMFLOAT3 spawnPosition = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
};

class EnemySpawner
{
public:
	EnemySpawner() = default;
	virtual ~EnemySpawner() = default;

public:
	void Initialize(const std::vector<EnemySpawnerPoolEntry>& spawnObjects);

	CGameObject* SpawnEnemy(
		int megaGridNumber,
		EEnemySpawnerEnemyKind kind
	);

	int SpawnEnemies(
	int megaGridNumber,
	EEnemySpawnerEnemyKind kind,
	int count
	);

	int PeekSpawnEntries(
		int megaGridNumber,
		EEnemySpawnerEnemyKind kind,
		int count,
		std::vector<EnemySpawnerPreviewEntry>& outEntries
	) const;

	CGameObject* SpawnPreviewEntry(
		const EnemySpawnerPreviewEntry& preview
	);

	int SpawnMegaGrid(int megaGridNumber);

	CGameObject* SpawnEnemyAt(
		int megaGridNumber,
		EEnemySpawnerEnemyKind kind,
		const DirectX::XMFLOAT3& position,
		float yawDeg
	);

	void RemoveEnemy(CGameObject* enemy);

public:
	const std::vector<CGameObject*>& GetActiveEnemies() const;
	int GetActiveEnemyCount() const;

private:
	CGameObject* ActivateEntry(
		size_t entryIndex,
		const DirectX::XMFLOAT3* overridePosition = nullptr,
		const float* overrideYawDeg = nullptr
	);

private:
	std::vector<EnemySpawnerPoolEntry> m_spawnEntries;
	std::vector<size_t> m_freeList;
	std::vector<CGameObject*> m_activeEnemies;
};