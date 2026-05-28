//-----------------------------------------------------------------------------
// File: GameSceneBillboard.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"

void CGameScene::ReleaseAllGameSceneEffectGpuResources()
{
	ReleaseItemBillboardGpuResources();
	ReleaseMuzzleFlashGpuResources();
	ReleaseBossPoisonProjectileGpuResources();
	ReleaseSwordTrailGpuResources();
	ReleaseMonsterSwordTrailGpuResources();
	ReleaseArrowTrailGpuResources();
	ReleaseBossCallSummonWwwGpuResources();
}