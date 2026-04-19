//-----------------------------------------------------------------------------
// File: GameSceneAttachmentBinder.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "GameSceneAttachmentBinder.h"

#include <algorithm>

#include "Object.h"
#include "LightComponent.h"
#include "FollowBoneComponent.h"
#include "PlayerEquipmentComponent.h"
#include "MonsterWeaponHitboxComponent.h"

namespace
{
	XMFLOAT4X4 BuildAttachmentOffsetMatrix(
		const GameSceneAttachmentTransformDesc& local)
	{
		const XMMATRIX S = XMMatrixScaling(
			local.scale.x,
			local.scale.y,
			local.scale.z
		);

		const XMMATRIX R = XMMatrixRotationRollPitchYaw(
			XMConvertToRadians(local.rotDeg.x),
			XMConvertToRadians(local.rotDeg.y),
			XMConvertToRadians(local.rotDeg.z)
		);

		const XMMATRIX T = XMMatrixTranslation(
			local.pos.x,
			local.pos.y,
			local.pos.z
		);

		XMFLOAT4X4 out{};
		XMStoreFloat4x4(&out, S * R * T);
		return out;
	}

	bool ResolveAttachmentPresetMatrix(
		EGameSceneAttachmentPresetId presetId,
		const char*& outBoneName,
		XMFLOAT4X4& outLocalOffset)
	{
		GameSceneAttachmentPresetDesc desc{};
		if ( !GetGameSceneAttachmentPresetDesc(presetId, desc) )
			return false;

		if ( !desc.boneName || !desc.boneName[0] )
			return false;

		outBoneName = desc.boneName;
		outLocalOffset = BuildAttachmentOffsetMatrix(desc.local);
		return true;
	}
}

void GameSceneAttachmentBinder::LinkSceneObjects(
	const LinkInput& input,
	std::vector<AttachmentBindSpec>& outAttachmentBinds)
{
	outAttachmentBinds.clear();

	static const std::array<CGameObject*, 4> kEmptyPlayers = { nullptr, nullptr, nullptr, nullptr };
	static const std::vector<CGameObject*> kEmptyRefs;

	const auto& playersBySlot = input.playersBySlot ? *input.playersBySlot : kEmptyPlayers;

	const auto& playerSwordRefs = input.playerSwordRefs ? *input.playerSwordRefs : kEmptyRefs;
	const auto& playerBowRefs = input.playerBowRefs ? *input.playerBowRefs : kEmptyRefs;
	const auto& playerAxeRefs = input.playerAxeRefs ? *input.playerAxeRefs : kEmptyRefs;
	const auto& playerGunRefs = input.playerGunRefs ? *input.playerGunRefs : kEmptyRefs;

	const auto& enemySwordRefs = input.enemySwordRefs ? *input.enemySwordRefs : kEmptyRefs;
	const auto& enemyBowRefs = input.enemyBowRefs ? *input.enemyBowRefs : kEmptyRefs;

	const auto& swordManRefs = input.swordManRefs ? *input.swordManRefs : kEmptyRefs;
	const auto& bowManRefs = input.bowManRefs ? *input.bowManRefs : kEmptyRefs;
	const auto& mutantRefs = input.mutantRefs ? *input.mutantRefs : kEmptyRefs;
	const auto& helmetRefs = input.helmetRefs ? *input.helmetRefs : kEmptyRefs;

	// ------------------------------------------------------------------------
	// Light follow target
	// ------------------------------------------------------------------------
	if ( input.playerSpotFollower )
	{
		CGameObject* local = input.preferredPlayer;
		if ( !local )
			local = playersBySlot[0];

		if ( local )
			input.playerSpotFollower->SetTarget(local);
	}

	// ------------------------------------------------------------------------
	// Attachment presets from authored catalog
	// ------------------------------------------------------------------------
	const char* playerSwordBone = nullptr;
	const char* playerBowBone = nullptr;
	const char* playerAxeBone = nullptr;
	const char* playerGunBone = nullptr;
	const char* enemySwordBone = nullptr;
	const char* enemyBowBone = nullptr;
	const char* mutantHelmetBone = nullptr;

	XMFLOAT4X4 playerSwordOffset{};
	XMFLOAT4X4 playerBowOffset{};
	XMFLOAT4X4 playerAxeOffset{};
	XMFLOAT4X4 playerGunOffset{};
	XMFLOAT4X4 enemySwordOffset{};
	XMFLOAT4X4 enemyBowOffset{};
	XMFLOAT4X4 mutantHelmetOffset{};

	ResolveAttachmentPresetMatrix(EGameSceneAttachmentPresetId::PlayerSword, playerSwordBone, playerSwordOffset);
	ResolveAttachmentPresetMatrix(EGameSceneAttachmentPresetId::PlayerBow, playerBowBone, playerBowOffset);
	ResolveAttachmentPresetMatrix(EGameSceneAttachmentPresetId::PlayerAxe, playerAxeBone, playerAxeOffset);
	ResolveAttachmentPresetMatrix(EGameSceneAttachmentPresetId::PlayerGun, playerGunBone, playerGunOffset);
	ResolveAttachmentPresetMatrix(EGameSceneAttachmentPresetId::EnemySword, enemySwordBone, enemySwordOffset);
	ResolveAttachmentPresetMatrix(EGameSceneAttachmentPresetId::EnemyBow, enemyBowBone, enemyBowOffset);
	ResolveAttachmentPresetMatrix(EGameSceneAttachmentPresetId::MutantHelmet, mutantHelmetBone, mutantHelmetOffset);

	{
		size_t helmetPairCount = helmetRefs.size();
		if ( mutantRefs.size() < helmetPairCount )
			helmetPairCount = mutantRefs.size();

		const size_t playerWeaponBindCount = static_cast< size_t >( input.playerCount ) * 4;
		const size_t enemyWeaponBindCount = enemySwordRefs.size() + enemyBowRefs.size();

		outAttachmentBinds.reserve(
			helmetPairCount +
			playerWeaponBindCount +
			enemyWeaponBindCount
		);
	}

	auto AddBind =
		[ &outAttachmentBinds ](
			CGameObject* follower,
			CGameObject* target,
			const char* boneName,
			const XMFLOAT4X4& localOffset)
		{
			if ( !follower || !target || !boneName || !boneName[0] )
				return;

			AttachmentBindSpec spec{};
			spec.follower = follower;
			spec.target = target;
			spec.boneName = boneName;
			spec.localOffset = localOffset;
			outAttachmentBinds.push_back(spec);
		};

	// ------------------------------------------------------------------------
	// Player weapon refs / equip object registration / optional offline loadout
	// ------------------------------------------------------------------------
	for ( int slot = 0; slot < static_cast< int >(input.playerCount); ++slot )
	{
		CGameObject* player = playersBySlot[( size_t ) slot];
		if ( !player ) continue;

		auto* equip = player->GetComponent<CPlayerEquipmentComponent>();
		if ( !equip ) continue;

		if ( ( size_t ) slot < playerSwordRefs.size() )
			equip->SetWeaponObject(EWeaponType::Sword, playerSwordRefs[( size_t ) slot]);

		if ( ( size_t ) slot < playerBowRefs.size() )
			equip->SetWeaponObject(EWeaponType::Bow, playerBowRefs[( size_t ) slot]);

		if ( ( size_t ) slot < playerAxeRefs.size() )
			equip->SetWeaponObject(EWeaponType::Axe, playerAxeRefs[( size_t ) slot]);

		if ( ( size_t ) slot < playerGunRefs.size() )
			equip->SetWeaponObject(EWeaponType::Gun, playerGunRefs[( size_t ) slot]);

#ifndef USING_NETWORK
		if ( input.applyOfflineTestLoadout )
		{
			switch ( slot )
			{
			case 0: equip->SetLoadout(EWeaponType::Sword); break;
			case 1: equip->SetLoadout(EWeaponType::Bow);   break;
			case 2: equip->SetLoadout(EWeaponType::Axe);   break;
			case 3: equip->SetLoadout(EWeaponType::Gun);   break;
			default: equip->ClearOwnedWeapons();           break;
			}
		}
#endif

		AddBind(equip->GetWeaponObject(EWeaponType::Sword), player, playerSwordBone, playerSwordOffset);
		AddBind(equip->GetWeaponObject(EWeaponType::Bow), player, playerBowBone, playerBowOffset);
		AddBind(equip->GetWeaponObject(EWeaponType::Axe), player, playerAxeBone, playerAxeOffset);
		AddBind(equip->GetWeaponObject(EWeaponType::Gun), player, playerGunBone, playerGunOffset);
	}

	// ------------------------------------------------------------------------
	// Enemy weapon binding
	// ------------------------------------------------------------------------
	{
		size_t pairCount = swordManRefs.size();
		if ( enemySwordRefs.size() < pairCount )
			pairCount = enemySwordRefs.size();

		for ( size_t i = 0; i < pairCount; ++i )
		{
			AddBind(enemySwordRefs[i], swordManRefs[i], enemySwordBone, enemySwordOffset);

			if ( auto* hitbox = enemySwordRefs[i]->GetComponent<CMonsterWeaponHitboxComponent>() )
			{
				hitbox->BindAttacker(swordManRefs[i]);
				hitbox->SetAttackClipName("Attack");
				hitbox->SetActiveWindow(0.20f, 0.55f);
			}
		}
	}

	{
		size_t pairCount = bowManRefs.size();
		if ( enemyBowRefs.size() < pairCount )
			pairCount = enemyBowRefs.size();

		for ( size_t i = 0; i < pairCount; ++i )
		{
			AddBind(enemyBowRefs[i], bowManRefs[i], enemyBowBone, enemyBowOffset);
		}
	}

	// ------------------------------------------------------------------------
	// Mutant helmet binding
	// ------------------------------------------------------------------------
	{
		size_t pairCount = helmetRefs.size();
		if ( mutantRefs.size() < pairCount )
			pairCount = mutantRefs.size();

		for ( size_t i = 0; i < pairCount; ++i )
		{
			AddBind(helmetRefs[i], mutantRefs[i], mutantHelmetBone, mutantHelmetOffset);
		}
	}

	// ------------------------------------------------------------------------
	// Apply all binds
	// ------------------------------------------------------------------------
	for ( AttachmentBindSpec& spec : outAttachmentBinds )
	{
		if ( !spec.follower || !spec.target || spec.boneName.empty() )
			continue;

		CFollowBoneComponent* follow = spec.follower->GetComponent<CFollowBoneComponent>();
		if ( !follow )
			follow = spec.follower->AddComponent<CFollowBoneComponent>();

		follow->Bind(spec.target, spec.boneName, spec.localOffset);
		follow->SnapNow();
	}
}