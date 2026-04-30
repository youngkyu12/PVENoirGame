//-----------------------------------------------------------------------------
// File: GameSceneObjectFactory.h
//-----------------------------------------------------------------------------
#pragma once

#include "stdafx.h"
#include "ColliderComponent.h"
#include "ActorTagComponent.h"
#include "MonsterAnimTypes.h"
#include "GameSceneContentCatalog.h"
#include "HealthComponent.h"
#include "AttackPowerComponent.h"

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

class CGameObject;
class CMesh;
struct CB_GAMEOBJECT_INFO;

namespace GameSceneObjectFactory
{
	struct CreateContext
	{
		ID3D12Device* device = nullptr;
		ID3D12GraphicsCommandList* cmd = nullptr;

		CB_GAMEOBJECT_INFO* mappedGameObjectCB = nullptr;
		D3D12_GPU_DESCRIPTOR_HANDLE cbvGpuHandle = {};
	};

	struct MonsterBoneWeaponConfigDesc
	{
		std::string clipName;
		float activeStart = 0.20f;
		float activeEnd = 0.55f;
		std::vector<std::string> boneNames;
	};

	struct StaticRenderableDesc
	{
		CreateContext ctx{};

		std::shared_ptr<CMesh> mesh;
		XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
		float yawDeg = 0.0f;

		bool spawnHidden = false;
		bool addStaticMeshRenderer = true;

		bool addCollider = false;
		EColliderType colliderType = EColliderType::OOBB;
		bool configureColliderFiltering = true;
		uint32_t colliderLayer = 0;
		uint32_t colliderMask = 0;
		bool colliderEnabled = true;

		const std::unordered_map<std::string, std::vector<AuthoredSubMeshOOBB>>* authoredStaticSubMeshOOBBs = nullptr;

		bool debugColliderBuildLog = false;
		std::string debugColliderAssetName;
		std::string debugColliderObjectName;

		bool addArrowComponent = false;
		bool addBulletComponent = false;
		bool addMonsterWeaponHitbox = false;

		bool addAttackPower = false;
		int attackPower = 0;
	};

	struct SkinnedRenderableDesc
	{
		CreateContext ctx{};

		std::shared_ptr<CMesh> mesh;
		XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
		float yawDeg = 0.0f;

		bool spawnHidden = false;
		bool addSkinnedMeshRenderer = true;
		bool enableSkinning = true;

		bool addCollider = false;
		EColliderType colliderType = EColliderType::BCapsule;
		bool configureColliderFiltering = true;
		uint32_t colliderLayer = 0;
		uint32_t colliderMask = 0;
		bool colliderEnabled = true;

		bool addAnimator = false;
		bool addActorTag = false;
		EActorKind actorKind = EActorKind::NPC;
		EPlayerControl playerControl = EPlayerControl::None;
		int playerSlot = -1;

		bool addPlayerController = false;
		bool addPlayerEquipment = false;
		bool addPlayerWeaponHitbox = false;

		bool addMonsterCombat = false;
		bool addMonsterWeaponHitbox = false;

		bool addHealth = false;
		int maxHp = 1;

		bool addAttackPower = false;
		int attackPower = 0;

		const char* skeletonKey = nullptr;
		const std::vector<GameSceneClipEntry>* clipEntries = nullptr;

		bool initPlayerController = false;
		const char* playerIdleClip = nullptr;
		const char* playerMoveClip = nullptr;
		const char* playerHitClip = nullptr;
		const char* playerAttackClip = nullptr;

		bool initMonsterController = false;
		MonsterAnimProfile monsterProfile{};
		EMonsterAnimState monsterInitialState = EMonsterAnimState::Idle;

		bool useOwnerBoneWeaponCapsules = false;
		std::vector<MonsterBoneWeaponConfigDesc> monsterWeaponConfigs;
	};

	void PreloadClipSet(
		CMesh* mesh,
		const char* skeletonKey,
		const std::vector<GameSceneClipEntry>& clipList
	);

	std::unique_ptr<CGameObject> CreateStaticRenderable(const StaticRenderableDesc& desc);
	std::unique_ptr<CGameObject> CreateSkinnedRenderable(const SkinnedRenderableDesc& desc);
}