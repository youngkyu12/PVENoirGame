//-----------------------------------------------------------------------------
// File: GameSceneBossCombatEffectCasts.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"

void CGameScene::ApplyBossMeleeSlashPlayerHits()
{
#ifndef USING_NETWORK
	// Use the full main-effect rectangle, including its transparent margins.
	// Match its rendered size and add depth along the plane normal.
	constexpr float kHitStartRatio = 0.07f; // End of birthFade in Billboard.hlsl.
	constexpr float kHitEndRatio = 0.72f;   // Start of lifeFade in Billboard.hlsl.
	constexpr float kHitDepth = 6.0f;

	for ( MuzzleFlashEntry& flash : m_muzzleFlashEffect.entries )
	{
		if ( !flash.active || flash.kind != EMuzzleFlashKind::BossMeleeSlash || !flash.meleeOwner )
			continue;

		CGameObject* boss = flash.meleeOwner;
		if ( !boss->GetActive() || IsMonsterDead(boss) )
		{
			flash.meleeOwner = nullptr;
			continue;
		}

		const float ratio = flash.lifetime > 0.0f ? flash.age / flash.lifetime : 1.0f;
		if ( ratio < kHitStartRatio || ratio >= kHitEndRatio )
			continue;

		auto* attack = boss->GetComponent<CAttackPowerComponent>();
		if ( !attack || attack->GetAttackPower() <= 0 )
			continue;

		BoundingOrientedBox box{};
		box.Center = flash.position;
		box.Extents = XMFLOAT3(
			(flash.startWidth + (flash.endWidth - flash.startWidth) * ratio) * 0.5f,
			(flash.startHeight + (flash.endHeight - flash.startHeight) * ratio) * 0.5f,
			kHitDepth * 0.5f);
		const XMMATRIX orientation(
			XMLoadFloat3(&flash.axisRight), XMLoadFloat3(&flash.axisUp),
			XMLoadFloat3(&flash.axisForward), XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f));
		XMStoreFloat4(&box.Orientation, XMQuaternionNormalize(XMQuaternionRotationMatrix(orientation)));

		for ( int slot = 0; slot < 4; ++slot )
		{
			if ( flash.meleeHitPlayerSlots[slot] )
				continue;
			CGameObject* player = GetPlayerBySlot(slot);
			if ( !player || !player->GetActive() ||
				(slot == m_localPlayerSlot && m_bLocalPlayerDead) ||
				IsBossPoisonProjectilePlayerRollInvincible(player) )
				continue;

			auto* collider = player->GetComponent<CColliderComponent>();
			auto* hp = player->GetComponent<CHealthComponent>();
			if ( !collider || !collider->IsEnabled() || !collider->IsCollisionEnabled() ||
				!hp || hp->IsDead() || !collider->IntersectsBoneCapsulesHierarchical(box) )
				continue;
			if ( !hp->TakeDamage(attack->GetAttackPower()) )
				continue;

			flash.meleeHitPlayerSlots[slot] = true;
			SpawnBloodSplash(player, nullptr, &flash.axisForward);
			if ( hp->IsDead() && slot == m_localPlayerSlot )
			{
				BeginLocalPlayerDeath(player);
				continue;
			}

			auto* animComp = player->GetComponent<CAnimatorComponent>();
			auto* ctrl = animComp ? animComp->EnsureController() : player->GetAnimController();
			if ( ctrl )
			{
				if ( hp->IsDead() ) ctrl->RequestDeath();
				else ctrl->RequestHit();
			}
		}
	}
#endif
}

void CGameScene::UpdateBossMeleeSlashCasts(float dt)
{
	if ( dt < 0.0f )
		dt = 0.0f;

	for ( CGameObject* boss : m_bossRefs )
	{
		if ( !boss )
			continue;

		if ( !boss->GetActive() || IsMonsterDead(boss) )
		{
			m_bossMeleeSlashCastStates.erase(boss);
			continue;
		}

		CAnimatorComponent* animComp =
			boss->GetComponent<CAnimatorComponent>();

		if ( !animComp )
		{
			m_bossMeleeSlashCastStates.erase(boss);
			continue;
		}

		CMonsterAnimController* ctrl =
			animComp->EnsureMonsterController();

		if ( !ctrl )
		{
			m_bossMeleeSlashCastStates.erase(boss);
			continue;
		}

		const bool isMeleePhase =
			ctrl->IsAttackPrimaryPhase() ||
			ctrl->IsAttackChainPhase();

		BossMeleeSlashCastState& state =
			m_bossMeleeSlashCastStates[boss];

		if ( !isMeleePhase )
		{
			state = BossMeleeSlashCastState{};
			continue;
		}

		if ( !state.wasMeleePhase )
		{
			state.wasMeleePhase = true;
			state.pendingSpawn = true;
			state.spawned = false;
			state.meleeAgeSec = 0.0f;

			RequestBossAttackSfx(boss);
		}
		else
		{
			state.meleeAgeSec += dt;
		}

		if ( state.pendingSpawn &&
			!state.spawned &&
			 state.meleeAgeSec >= kBossMeleeSlashLaunchDelaySec )
		{
			SpawnBossMeleeSlashEffect(boss);

			state.spawned = true;
			state.pendingSpawn = false;
		}
	}
}
