#include "stdafx.h"
#include "EnemyPool.h"
#include "Object.h"
#include "AssetManager.h"
#include "GameSceneContentCatalog.h"
#include "GameSceneObjectFactory.h"
#include "Scene.h"

void EnemyPool::Initialize(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, const std::shared_ptr<CSkinnedObjectsShader>& pSkinnedShader, int cnt)
{
	if ( !dev || !cmd || !pSkinnedShader )
		return;

	shader = pSkinnedShader;

	if ( count == 0 )
		count = kDefaultCapacity;

	capacity = count;

	cbElementBytes = ( ( sizeof(CB_GAMEOBJECT_INFO) + 255 ) & ~255 );

	cbGameObjects = ::CreateBufferResource(
		dev, cmd, nullptr,
		cbElementBytes * capacity,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	if ( !cbGameObjects )
		return;

	cbGameObjects->Map(0, nullptr, ( void** ) &mappedGameObjects);

	if ( !CScene::m_pDescriptorHeap )
		return;

	baseCbvGpu = CScene::m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
	cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	CScene::m_pDescriptorHeap->CreateConstantBufferViews(
		dev,
		capacity,
		cbGameObjects.Get(),
		cbElementBytes
	);

	mEnemies.clear();
	mEnemies.reserve(capacity);

	while ( !mFreeEnemies.empty() )
		mFreeEnemies.pop();

	AssetBuildDesc enemyDesc{};
	if ( !GetGameSceneAssetBuildDesc(EGameSceneAssetId::SwordMan, enemyDesc) )
		return;

	BuiltAsset enemyAsset = AssetManager::BuildAsset(
		dev, cmd,
		nullptr,
		enemyDesc
	);

	const auto& enemyClips = GetEnemySwordClipEntries();
	GameSceneObjectFactory::PreloadClipSet(
		enemyAsset.mesh.get(),
		"Enemy",
		enemyClips
	);

	for ( UINT i = 0; i < capacity; ++i )
	{
		GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
		createDesc.ctx.device = dev;
		createDesc.ctx.cmd = cmd;
		createDesc.ctx.mappedGameObjectCB = reinterpret_cast< CB_GAMEOBJECT_INFO* >(
			reinterpret_cast< UINT8* >(mappedGameObjects) + i * cbElementBytes
		);
		createDesc.mesh = enemyAsset.mesh;
		createDesc.spawnHidden = true;
		createDesc.addAnimator = true;
		createDesc.skeletonKey = "Enemy";
		createDesc.clipEntries = &enemyClips;

		auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
		if ( !obj )
			continue;

		obj->SetActive(false);
		mFreeEnemies.push(obj.get());
		mEnemies.push_back(std::move(obj));
	}

	count = static_cast< UINT >( mEnemies.size() );
	capacity = count;

}

CGameObject* EnemyPool::Acquire()
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

void EnemyPool::Release(CGameObject* enemy)
{
	if ( enemy == nullptr )
		return;

	enemy->Reset();
	enemy->SetActive(false);

	mFreeEnemies.push(enemy);
}
