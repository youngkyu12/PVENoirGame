#include "stdafx.h"
#include "EnemyPool.h"
#include "Object.h"

void EnemyPool::Initialize(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, const std::shared_ptr<CSkinnedObjectsShader>& pSkinnedShader, UINT nRenderTargets, DXGI_FORMAT* rtvFormats, DXGI_FORMAT dsvFormat)
{
	mEnemies.resize(count);

}