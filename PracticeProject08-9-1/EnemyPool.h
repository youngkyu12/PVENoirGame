#pragma once


class EnemyPool
{
public:
	void Initialize(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		const std::shared_ptr<CSkinnedObjectsShader>& pSkinnedShader,
		UINT nRenderTargets,
		DXGI_FORMAT* rtvFormats,
		DXGI_FORMAT dsvFormat);

	CGameObject* Acquire()
	{
		if ( mFreeEnemies.empty() )
		{
			return nullptr;
		}

		CGameObject* enemy = mFreeEnemies.front();
		mFreeEnemies.pop();

		enemy->SetActive(true);
		return enemy;
	}

	void Release(CGameObject* enemy)
	{
		if ( enemy == nullptr )
			return;

		enemy->Reset();
		enemy->SetActive(false);

		mFreeEnemies.push(enemy);
	}

	int GetFreeCount() const
	{
		return static_cast< int >( mFreeEnemies.size() );
	}

private:
	std::shared_ptr<CSkinnedObjectsShader> shader;

    UINT capacity = 0;
    UINT count = 0;

    UINT cbElementBytes = 0;

    ComPtr<ID3D12Resource> cbGameObjects;
    CB_GAMEOBJECT_INFO* mappedGameObjects = nullptr;

    D3D12_GPU_DESCRIPTOR_HANDLE baseCbvGpu = { 0 };
    UINT cbvInc = 0;

	std::vector<CGameObject> mEnemies;
	std::queue<CGameObject*> mFreeEnemies;
};

