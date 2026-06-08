//-----------------------------------------------------------------------------
// File: GameSceneBillboard.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"

void CGameScene::ReleaseAllGameSceneEffectGpuResources()
{
	ReleaseItemBillboardGpuResources();
	ReleaseMonsterHpGaugeGpuResources();
	ReleaseMuzzleFlashGpuResources();
	ReleaseGunSmokeGpuResources();
	ReleaseBossPoisonProjectileGpuResources();
	ReleaseSwordTrailGpuResources();
	ReleaseMonsterSwordTrailGpuResources();
	ReleaseArrowTrailGpuResources();
	ReleaseMonsterArrowTrailGpuResources();
	ReleaseBossCallSummonWwwGpuResources();
}