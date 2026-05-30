//-----------------------------------------------------------------------------
// File: GameSceneInventory.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"
#include "GameSceneHelper.h"

using namespace GameSceneHelper;

void CGameScene::InitializeInventoryItemCounts()
{
#ifdef USING_NETWORK
	m_inventoryItemCounts = { 0, 0, 0, 0 };
#else
	m_inventoryItemCounts = { 10, 10, 10, 10 };
#endif
}

bool CGameScene::TryBeginLocalPlayerMoveSpeedPotion()
{
#ifndef USING_NETWORK
	if ( m_bLocalPlayerDead )
		return false;

	CGameObject* localPlayer = GetPlayer();
	if ( !localPlayer )
		localPlayer = GetPlayerBySlot(0);

	if ( !localPlayer )
		return false;

	CPlayerControllerComponent* pc = localPlayer->GetComponent<CPlayerControllerComponent>();
	if ( !pc )
		return false;

	if ( !m_bMoveSpeedPotionActive )
	{
		m_moveSpeedPotionOriginalWalkSpeed = pc->GetWalkMoveSpeed();
		m_moveSpeedPotionOriginalRunSpeed = pc->GetRunMoveSpeed();
	}

	pc->SetMoveSpeeds(kMoveSpeedPotionWalkSpeed, kMoveSpeedPotionRunSpeed);

	m_bMoveSpeedPotionActive = true;
	m_moveSpeedPotionRemainingSec = kMoveSpeedPotionDurationSec;
	m_moveSpeedPotionLastLoggedSecond = static_cast< int >( kMoveSpeedPotionDurationSec );

	char buf[192];
	sprintf_s(buf, "[Inventory][SpeedPotion] start remaining=%d walk=%.1f run=%.1f\n", m_moveSpeedPotionLastLoggedSecond, kMoveSpeedPotionWalkSpeed, kMoveSpeedPotionRunSpeed);
	OutputDebugStringA(buf);

	return true;
#else
	return false;
#endif
}

void CGameScene::UpdateLocalPlayerMoveSpeedPotion(float dt)
{
#ifndef USING_NETWORK
	if ( !m_bMoveSpeedPotionActive )
		return;

	if ( dt < 0.0f )
		dt = 0.0f;

	m_moveSpeedPotionRemainingSec -= dt;

	if ( m_moveSpeedPotionRemainingSec <= 0.0f )
	{
		RestoreLocalPlayerMoveSpeedPotion();
		return;
	}

	const int remainingSecond = static_cast< int >( m_moveSpeedPotionRemainingSec + 0.999f );

	if ( remainingSecond != m_moveSpeedPotionLastLoggedSecond )
	{
		m_moveSpeedPotionLastLoggedSecond = remainingSecond;

		char buf[128];
		sprintf_s(buf, "[Inventory][SpeedPotion] remaining=%d\n", remainingSecond);
		OutputDebugStringA(buf);
	}
#else
	UNREFERENCED_PARAMETER(dt);
#endif
}

void CGameScene::RestoreLocalPlayerMoveSpeedPotion()
{
#ifndef USING_NETWORK
	CGameObject* localPlayer = GetPlayer();
	if ( !localPlayer )
		localPlayer = GetPlayerBySlot(0);

	if ( localPlayer )
	{
		CPlayerControllerComponent* pc = localPlayer->GetComponent<CPlayerControllerComponent>();
		if ( pc )
			pc->SetMoveSpeeds(m_moveSpeedPotionOriginalWalkSpeed, m_moveSpeedPotionOriginalRunSpeed);
	}

	m_bMoveSpeedPotionActive = false;
	m_moveSpeedPotionRemainingSec = 0.0f;
	m_moveSpeedPotionLastLoggedSecond = -1;

	char buf[160];
	sprintf_s(buf, "[Inventory][SpeedPotion] expired restore walk=%.1f run=%.1f\n", m_moveSpeedPotionOriginalWalkSpeed, m_moveSpeedPotionOriginalRunSpeed);
	OutputDebugStringA(buf);
#endif
}

bool CGameScene::TryBeginLocalPlayerAttackPowerPotion()
{
#ifndef USING_NETWORK
	if ( m_bLocalPlayerDead )
		return false;

	CGameObject* localPlayer = GetPlayer();
	if ( !localPlayer )
		localPlayer = GetPlayerBySlot(0);

	if ( !localPlayer )
		return false;

	m_bAttackPowerPotionActive = true;
	m_attackPowerPotionRemainingSec = kAttackPowerPotionDurationSec;
	m_attackPowerPotionLastLoggedSecond = static_cast< int >( kAttackPowerPotionDurationSec );

	RefreshPlayerWeaponAttackPowers();

	char buf[192];
	sprintf_s(buf, "[Inventory][AttackPotion] start remaining=%d multiplier=%d\n", m_attackPowerPotionLastLoggedSecond, kAttackPowerPotionMultiplier);
	OutputDebugStringA(buf);

	return true;
#else
	return false;
#endif
}

void CGameScene::UpdateLocalPlayerAttackPowerPotion(float dt)
{
#ifndef USING_NETWORK
	if ( !m_bAttackPowerPotionActive )
		return;

	if ( dt < 0.0f )
		dt = 0.0f;

	m_attackPowerPotionRemainingSec -= dt;

	if ( m_attackPowerPotionRemainingSec <= 0.0f )
	{
		RestoreLocalPlayerAttackPowerPotion();
		return;
	}

	const int remainingSecond = static_cast< int >( m_attackPowerPotionRemainingSec + 0.999f );

	if ( remainingSecond != m_attackPowerPotionLastLoggedSecond )
	{
		m_attackPowerPotionLastLoggedSecond = remainingSecond;

		char buf[128];
		sprintf_s(buf, "[Inventory][AttackPotion] remaining=%d\n", remainingSecond);
		OutputDebugStringA(buf);
	}
#else
	UNREFERENCED_PARAMETER(dt);
#endif
}

void CGameScene::RestoreLocalPlayerAttackPowerPotion()
{
#ifndef USING_NETWORK
	if ( !m_bAttackPowerPotionActive )
		return;

	m_bAttackPowerPotionActive = false;
	m_attackPowerPotionRemainingSec = 0.0f;
	m_attackPowerPotionLastLoggedSecond = -1;

	RefreshPlayerWeaponAttackPowers();

	OutputDebugStringA("[Inventory][AttackPotion] expired restore attack power\n");
#endif
}

int CGameScene::ApplyLocalPlayerAttackPowerPotionMultiplier(int attackPower) const
{
#ifndef USING_NETWORK
	if ( m_bAttackPowerPotionActive )
		return attackPower * kAttackPowerPotionMultiplier;
#endif

	return attackPower;
}

bool CGameScene::TryBeginLocalPlayerDefensePotion()
{
#ifndef USING_NETWORK
	if ( m_bLocalPlayerDead )
		return false;

	CGameObject* localPlayer = GetPlayer();
	if ( !localPlayer )
		localPlayer = GetPlayerBySlot(0);

	if ( !localPlayer )
		return false;

	CHealthComponent* hp = localPlayer->GetComponent<CHealthComponent>();
	if ( !hp )
		return false;

	if ( hp->IsDead() )
		return false;

	if ( !m_bDefensePotionActive )
		m_defensePotionOriginalIncomingDamageScale = hp->GetIncomingDamageScale();

	hp->SetIncomingDamageScale(kDefensePotionIncomingDamageScale);

	m_bDefensePotionActive = true;
	m_defensePotionRemainingSec = kDefensePotionDurationSec;
	m_defensePotionLastLoggedSecond = static_cast< int >( kDefensePotionDurationSec );

	char buf[192];
	sprintf_s(buf, "[Inventory][DefensePotion] start remaining=%d incomingDamageScale=%.2f\n", m_defensePotionLastLoggedSecond, kDefensePotionIncomingDamageScale);
	OutputDebugStringA(buf);

	return true;
#else
	return false;
#endif
}

void CGameScene::UpdateLocalPlayerDefensePotion(float dt)
{
#ifndef USING_NETWORK
	if ( !m_bDefensePotionActive )
		return;

	if ( dt < 0.0f )
		dt = 0.0f;

	m_defensePotionRemainingSec -= dt;

	if ( m_defensePotionRemainingSec <= 0.0f )
	{
		RestoreLocalPlayerDefensePotion();
		return;
	}

	const int remainingSecond = static_cast< int >( m_defensePotionRemainingSec + 0.999f );

	if ( remainingSecond != m_defensePotionLastLoggedSecond )
	{
		m_defensePotionLastLoggedSecond = remainingSecond;

		char buf[128];
		sprintf_s(buf, "[Inventory][DefensePotion] remaining=%d\n", remainingSecond);
		OutputDebugStringA(buf);
	}
#else
	UNREFERENCED_PARAMETER(dt);
#endif
}

void CGameScene::RestoreLocalPlayerDefensePotion()
{
#ifndef USING_NETWORK
	if ( !m_bDefensePotionActive )
		return;

	CGameObject* localPlayer = GetPlayer();
	if ( !localPlayer )
		localPlayer = GetPlayerBySlot(0);

	if ( localPlayer )
	{
		CHealthComponent* hp = localPlayer->GetComponent<CHealthComponent>();
		if ( hp )
			hp->SetIncomingDamageScale(m_defensePotionOriginalIncomingDamageScale);
	}

	m_bDefensePotionActive = false;
	m_defensePotionRemainingSec = 0.0f;
	m_defensePotionOriginalIncomingDamageScale = 1.0f;
	m_defensePotionLastLoggedSecond = -1;

	OutputDebugStringA("[Inventory][DefensePotion] expired restore incoming damage scale\n");
#endif
}

void CGameScene::SetInventoryItemCounts(const std::array<int, CGameSceneHUD::kInventorySlotCount>& counts)
{
	for ( int i = 0; i < CGameSceneHUD::kInventorySlotCount; ++i )
		m_inventoryItemCounts[i] = counts[i] < 0 ? 0 : counts[i];

	m_hud.SetInventoryItemCounts(m_inventoryItemCounts);
}

bool CGameScene::RequestUseInventoryItemSlot(int slot)
{
	if ( slot < 0 || slot >= CGameSceneHUD::kInventorySlotCount )
		return false;

#ifdef USING_NETWORK
	// 네트워크 모드에서는 클라이언트가 직접 HP/수량을 변경하지 않는다.
	// 현재 Framework::ProcessInput()에서 bit 10~13으로 사용 요청은 서버에 전달되므로, 서버가 승인한 결과를 SetInventoryItemCounts() 및 서버 HP 동기화로 반영하면 된다.
	return false;
#else
	if ( m_inventoryItemCounts[static_cast< size_t >(slot)] <= 0 )
		return false;

	bool itemEffectApplied = false;

	switch ( slot )
	{
	case 0:
	{
		constexpr int kHealPotionRecoverAmount = 20;

		CGameObject* localPlayer = GetPlayer();
		if ( !localPlayer )
			localPlayer = GetPlayerBySlot(0);

		if ( !localPlayer )
			return false;

		if ( m_bLocalPlayerDead )
			return false;

		CHealthComponent* hp = localPlayer->GetComponent<CHealthComponent>();
		if ( !hp )
			return false;

		if ( hp->IsDead() )
			return false;

		if ( hp->GetCurrentHp() >= hp->GetMaxHp() )
			return false;

		const int hpBefore = hp->GetCurrentHp();
		hp->Heal(kHealPotionRecoverAmount);
		itemEffectApplied = ( hp->GetCurrentHp() > hpBefore );
		break;
	}

	case 1:
		itemEffectApplied = TryBeginLocalPlayerAttackPowerPotion();
		break;

	case 2:
		itemEffectApplied = TryBeginLocalPlayerDefensePotion();
		break;

	case 3:
		itemEffectApplied = TryBeginLocalPlayerMoveSpeedPotion();
		break;

	default:
		return false;
	}

	if ( !itemEffectApplied )
		return false;

	--m_inventoryItemCounts[static_cast< size_t >( slot )];
	m_hud.SetInventoryItemCounts(m_inventoryItemCounts);

	char buf[160];
	sprintf_s(buf, "[Inventory] Use slot=%d remain=%d\n", slot, m_inventoryItemCounts[static_cast< size_t >( slot )]);
	OutputDebugStringA(buf);

	return true;
#endif
}
