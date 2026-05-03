#include "stdafx.h"
#include "EnemyPool.h"
#include "Object.h"
#include "AssetManager.h"
#include "GameSceneContentCatalog.h"
#include "GameSceneObjectFactory.h"
#include "Scene.h"

namespace
{
	enum : uint32_t
	{
		kCollisionLayerMonster = 1,
		kCollisionLayerPlayerWeapon = 3
	};

	static constexpr uint32_t CollisionBit(uint32_t layer)
	{
		return ( 1u << layer );
	}
}

void EnemyPool::Initialize(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, const std::shared_ptr<CSkinnedObjectsShader>& pSkinnedShader, int cnt)
{
	if ( !dev || !cmd || !pSkinnedShader )
		return;

	shader = pSkinnedShader;

	if ( cnt > 0 )
		count = static_cast< UINT >( cnt );
	else
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
	if ( !GetGameSceneAssetBuildDesc(EGameSceneAssetId::Ghoul, enemyDesc) )
		return;

	BuiltAsset enemyAsset = AssetManager::BuildAsset(
		dev, cmd,
		nullptr,
		enemyDesc
	);

	const auto& enemyClips = GetGhoulClipEntries();
	GameSceneObjectFactory::PreloadClipSet(
		enemyAsset.mesh.get(),
		"Ghoul",
		enemyClips
	);

	auto ApplyMonsterBodyCollider =
		[ ] (GameSceneObjectFactory::SkinnedRenderableDesc& desc)
		{
			desc.addCollider = true;
			desc.colliderType = EColliderType::BCapsule;
			desc.colliderLayer = kCollisionLayerMonster;
			desc.colliderMask = CollisionBit(kCollisionLayerPlayerWeapon);
			desc.colliderEnabled = true;
		};

	for ( UINT i = 0; i < capacity; ++i )
	{
		GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
		createDesc.ctx.device = dev;
		createDesc.ctx.cmd = cmd;
		createDesc.ctx.mappedGameObjectCB = reinterpret_cast< CB_GAMEOBJECT_INFO* >(
			reinterpret_cast< UINT8* >(mappedGameObjects) + i * cbElementBytes
		);
		createDesc.ctx.cbvGpuHandle.ptr =
			baseCbvGpu.ptr + static_cast< UINT64 >(i) * cbvInc;
		createDesc.mesh = enemyAsset.mesh;
		createDesc.spawnHidden = true;
		createDesc.addCollider = true;
		createDesc.addAnimator = true;
		createDesc.addActorTag = true;
		createDesc.actorKind = EActorKind::NPC;
		createDesc.playerControl = EPlayerControl::None;
		createDesc.playerSlot = -1;
		createDesc.addMonsterCombat = true;
		createDesc.addMonsterWeaponHitbox = true;
		createDesc.skeletonKey = "Ghoul";
		createDesc.clipEntries = &enemyClips;
		createDesc.initMonsterController = true;
		createDesc.monsterInitialState = EMonsterAnimState::Idle;
		createDesc.monsterProfile.idleClip = "Idle";
		createDesc.monsterProfile.moveClip = "Walk";
		createDesc.monsterProfile.runClip = "Run";
		createDesc.monsterProfile.hitClip = "Hit";
		createDesc.monsterProfile.attackClip = "Attack";
		createDesc.monsterProfile.deathClip = "Death";
		createDesc.useOwnerBoneWeaponCapsules = true;
		createDesc.monsterWeaponConfigs.push_back(
			{ "Attack", 0.20f, 0.55f, { "hand_r" } }
		);
		ApplyMonsterBodyCollider(createDesc);
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
