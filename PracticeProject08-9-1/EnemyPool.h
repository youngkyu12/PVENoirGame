#pragma once

#include "stdafx.h"
#include "Object.h"

#include <memory>
#include <queue>
#include <vector>

class EnemyPool
{
public:
	void Initialize(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		const std::shared_ptr<CSkinnedObjectsShader>& pSkinnedShader,
		int cnt);

	CGameObject* Acquire();

	void Release(CGameObject* enemy);

	int GetFreeCount() const
	{
		return static_cast< int >( mFreeEnemies.size() );
	}

private:
	static constexpr UINT kDefaultCapacity = 32;

	std::shared_ptr<CSkinnedObjectsShader> shader;

	UINT capacity = kDefaultCapacity;
	UINT count = kDefaultCapacity;

    UINT cbElementBytes;

    ComPtr<ID3D12Resource> cbGameObjects;
    CB_GAMEOBJECT_INFO* mappedGameObjects = nullptr;

    D3D12_GPU_DESCRIPTOR_HANDLE baseCbvGpu = { 0 };
    UINT cbvInc = 0;

	std::vector<std::unique_ptr<CGameObject>> mEnemies;
	std::queue<CGameObject*> mFreeEnemies;
};

