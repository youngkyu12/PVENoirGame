//-----------------------------------------------------------------------------
// File: GameSceneBossCombatEffectCasts.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"

void CGameScene::UpdateBossMeleeSlashCasts(float dt)
{
#ifndef USING_NETWORK
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
#else
	UNREFERENCED_PARAMETER(dt);
#endif
}