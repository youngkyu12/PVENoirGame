//-----------------------------------------------------------------------------
// File: GameSceneObjectFactory.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "GameSceneObjectFactory.h"

#include "AssetManager.h"
#include "Object.h"
#include "Mesh.h"
#include "AnimController.h"
#include "MonsterAnimController.h"

#include "AnimatorComponent.h"
#include "PlayerControllerComponent.h"
#include "PlayerEquipmentComponent.h"
#include "WeaponHitboxComponent.h"
#include "MonsterWeaponHitboxComponent.h"
#include "MonsterCombatComponent.h"

#include "ArrowComponent.h"
#include "BulletComponent.h"
#include "HealthComponent.h"
#include "AttackPowerComponent.h"

namespace
{
	void AddCachedClipSetToAnimator(
		CAnimatorComponent* animComp,
		CMesh* mesh,
		const char* skeletonKey,
		const std::vector<GameSceneClipEntry>& clipList)
	{
		if ( !animComp || !mesh || !skeletonKey )
			return;

		for ( const auto& clipInfo : clipList )
		{
			AnimationClip clip{};
			if ( AssetManager::LoadCachedClip(
				mesh,
				skeletonKey,
				clipInfo.filePath,
				clipInfo.clipName,
				clip,
				1.0f) )
			{
				animComp->AddClip(clip);
			}
		}
	}
}

void GameSceneObjectFactory::PreloadClipSet(
	CMesh* mesh,
	const char* skeletonKey,
	const std::vector<GameSceneClipEntry>& clipList)
{
	if ( !mesh || !skeletonKey )
		return;

	for ( const auto& clipInfo : clipList )
	{
		AnimationClip clip{};
		AssetManager::LoadCachedClip(
			mesh,
			skeletonKey,
			clipInfo.filePath,
			clipInfo.clipName,
			clip,
			1.0f
		);
	}
}

std::unique_ptr<CGameObject> GameSceneObjectFactory::CreateStaticRenderable(const StaticRenderableDesc& desc)
{
	if ( !desc.ctx.device || !desc.ctx.cmd )
		return nullptr;

	if ( !desc.mesh )
		return nullptr;

	auto obj = std::make_unique<CGameObject>(1);

	if ( desc.ctx.mappedGameObjectCB )
		obj->SetMappedGameObjectCB(desc.ctx.mappedGameObjectCB);

	obj->SetMesh(0, desc.mesh);

	if ( desc.addStaticMeshRenderer )
		obj->AddComponent<CStaticMeshRendererComponent>();

	if ( desc.addCollider )
	{
		auto* collider = obj->AddComponent<CColliderComponent>(desc.colliderType);
		if ( collider )
		{
			if ( desc.configureColliderFiltering )
			{
				collider->SetLayer(desc.colliderLayer);
				collider->SetMask(desc.colliderMask);
			}

			collider->SetCollisionEnabled(desc.colliderEnabled);

			if ( desc.authoredStaticSubMeshOOBBs &&
				desc.colliderType == EColliderType::OOBB )
			{
				collider->SetStaticSubMeshAuthoredOOBBs(*desc.authoredStaticSubMeshOOBBs);
			}
		}
	}

	if ( desc.addArrowComponent )
		obj->AddComponent<CArrowComponent>();

	if ( desc.addBulletComponent )
		obj->AddComponent<CBulletComponent>();

	if ( desc.addMonsterWeaponHitbox )
		obj->AddComponent<CMonsterWeaponHitboxComponent>();

	if ( desc.addAttackPower )
	{
		auto* attack = obj->AddComponent<CAttackPowerComponent>();
		if ( attack )
			attack->SetAttackPower(desc.attackPower);
	}

	if ( desc.spawnHidden )
	{
		obj->SetPosition(0.0f, -10000.0f, 0.0f);
	}
	else
	{
		obj->SetPosition(desc.position);
		obj->Rotate(0.0f, desc.yawDeg, 0.0f);
	}

	obj->SetCbvGPUDescriptorHandlePtr(desc.ctx.cbvGpuHandle.ptr);
	obj->CreateComponents(desc.ctx.device, desc.ctx.cmd);

	return obj;
}

std::unique_ptr<CGameObject> GameSceneObjectFactory::CreateSkinnedRenderable(const SkinnedRenderableDesc& desc)
{
	if ( !desc.ctx.device || !desc.ctx.cmd )
		return nullptr;

	if ( !desc.mesh )
		return nullptr;

	auto obj = std::make_unique<CGameObject>(1);

	if ( desc.ctx.mappedGameObjectCB )
		obj->SetMappedGameObjectCB(desc.ctx.mappedGameObjectCB);

	obj->SetMesh(0, desc.mesh);

	if ( desc.addSkinnedMeshRenderer )
		obj->AddComponent<CSkinnedMeshRendererComponent>();

	if ( desc.addCollider )
	{
		auto* collider = obj->AddComponent<CColliderComponent>(desc.colliderType);
		if ( collider )
		{
			if ( desc.configureColliderFiltering )
			{
				collider->SetLayer(desc.colliderLayer);
				collider->SetMask(desc.colliderMask);
			}

			collider->SetCollisionEnabled(desc.colliderEnabled);
		}
	}

	CAnimatorComponent* animComp = nullptr;
	if ( desc.addAnimator )
		animComp = obj->AddComponent<CAnimatorComponent>();

	if ( desc.addPlayerEquipment )
		obj->AddComponent<CPlayerEquipmentComponent>();

	if ( desc.addPlayerWeaponHitbox )
		obj->AddComponent<CWeaponHitboxComponent>();

	if ( desc.addMonsterCombat )
		obj->AddComponent<CMonsterCombatComponent>();

	CMonsterWeaponHitboxComponent* monsterWeaponHitbox = nullptr;
	if ( desc.addMonsterWeaponHitbox )
		monsterWeaponHitbox = obj->AddComponent<CMonsterWeaponHitboxComponent>();

	if ( desc.addHealth )
	{
		auto* hp = obj->AddComponent<CHealthComponent>();
		if ( hp )
			hp->SetMaxHp(desc.maxHp, true);
	}

	if ( desc.addAttackPower )
	{
		auto* attack = obj->AddComponent<CAttackPowerComponent>();
		if ( attack )
			attack->SetAttackPower(desc.attackPower);
	}

	if ( desc.addActorTag )
	{
		auto* tag = obj->AddComponent<CActorTagComponent>();
		if ( tag )
		{
			tag->kind = desc.actorKind;
			tag->control = desc.playerControl;
			tag->playerSlot = desc.playerSlot;
		}
	}

	if ( desc.addPlayerController )
		obj->AddComponent<CPlayerControllerComponent>();

	if ( desc.spawnHidden )
	{
		obj->SetPosition(0.0f, -10000.0f, 0.0f);
	}
	else
	{
		obj->SetPosition(desc.position);
		obj->Rotate(0.0f, desc.yawDeg, 0.0f);
	}

	obj->SetCbvGPUDescriptorHandlePtr(desc.ctx.cbvGpuHandle.ptr);

	if ( desc.enableSkinning && desc.mesh->IsSkinnedMesh() )
	{
		obj->EnableSkinning(desc.ctx.device, desc.mesh->GetBoneCount());
	}

	if ( animComp && desc.skeletonKey && desc.clipEntries )
	{
		AddCachedClipSetToAnimator(
			animComp,
			desc.mesh.get(),
			desc.skeletonKey,
			*desc.clipEntries
		);
	}

	if ( animComp && desc.initPlayerController )
	{
		if ( auto* ctrl = animComp->EnsureController() )
		{
			ctrl->EnablePlayerClipSet(true);

			if ( desc.playerIdleClip ) ctrl->SetIdleClip(desc.playerIdleClip);
			if ( desc.playerMoveClip ) ctrl->SetMoveClip(desc.playerMoveClip);
			if ( desc.playerHitClip ) ctrl->SetHitClip(desc.playerHitClip);
			if ( desc.playerAttackClip ) ctrl->SetAttackClip(desc.playerAttackClip);

			ctrl->SetSpeed(0.0f);
			ctrl->SetMoveDirection(0);
			ctrl->SetRunRequested(false);
			ctrl->SetRollMoveSpeed(5.0f);
			ctrl->SetRollMoveWindow(0.08f, 0.55f);
			ctrl->Update(0.0f);
		}
	}

	if ( animComp && desc.initMonsterController )
	{
		if ( auto* ctrl = animComp->EnsureMonsterController() )
		{
			ctrl->SetProfile(desc.monsterProfile);
			ctrl->SetLocomotionState(desc.monsterInitialState);
			ctrl->Update(0.0f);
		}
	}

	if ( monsterWeaponHitbox )
	{
		monsterWeaponHitbox->BindAttacker(obj.get());

		if ( desc.useOwnerBoneWeaponCapsules )
			monsterWeaponHitbox->SetUseOwnerBoneWeaponCapsules(true);

		for ( const auto& cfg : desc.monsterWeaponConfigs )
		{
			monsterWeaponHitbox->AddBoneWeaponConfig(
				cfg.clipName.c_str(),
				cfg.activeStart,
				cfg.activeEnd,
				cfg.boneNames
			);
		}
	}

	obj->CreateComponents(desc.ctx.device, desc.ctx.cmd);

	if ( animComp )
		animComp->EvaluatePose(0.0f);

	return obj;
}