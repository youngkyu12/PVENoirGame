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

int CGameScene::ApplyPlayerAttackPowerPotionMultiplier(int playerSlot, int attackPower) const
{
#ifndef USING_NETWORK
	if ( playerSlot < 0 || playerSlot >= 4 )
		return attackPower;

	CInventoryComponent* inventory = GetInventoryByPlayerSlot(playerSlot);
	if ( inventory )
		return inventory->ApplyAttackPowerMultiplier(attackPower);
#else
	UNREFERENCED_PARAMETER(playerSlot);
#endif

	return attackPower;
}

void CGameScene::SetInventoryItemCounts(const std::array<int, CGameSceneHUD::kInventorySlotCount>& counts)
{
	for ( int i = 0; i < CGameSceneHUD::kInventorySlotCount; ++i )
		m_inventoryItemCounts[static_cast< size_t >(i)] = counts[static_cast< size_t >(i)] < 0 ? 0 : counts[static_cast< size_t >( i )];

	CInventoryComponent* inventory = GetLocalPlayerInventory();

	if ( inventory )
	{
		std::array<int, CInventoryComponent::kInventorySlotCount> inventoryCounts = { 0, 0, 0, 0 };
		const int copyCount = ( CInventoryComponent::kInventorySlotCount < CGameSceneHUD::kInventorySlotCount ) ? CInventoryComponent::kInventorySlotCount : CGameSceneHUD::kInventorySlotCount;

		for ( int i = 0; i < copyCount; ++i )
			inventoryCounts[static_cast< size_t >(i)] = m_inventoryItemCounts[static_cast< size_t >(i)];

		inventory->SetItemCounts(inventoryCounts);
	}

	SyncLocalInventoryToHud();
}

bool CGameScene::RequestUseInventoryItemSlot(int slot)
{
	if ( slot < 0 || slot >= CGameSceneHUD::kInventorySlotCount )
		return false;

#ifdef USING_NETWORK
	if ( m_bLocalPlayerDead )
		return false;

	if ( m_inventoryItemCounts[static_cast<size_t>(slot)] <= 0 )
		return false;

	{
		Protocol::C_USE_ITEM pkt;
		pkt.set_playerid(g_myPlayerId);
		pkt.set_slot(slot);
		auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
		g_clientService->BroadCast(sendBuffer);
	}

	std::array<int, CGameSceneHUD::kInventorySlotCount> counts = m_inventoryItemCounts;
	--counts[static_cast<size_t>(slot)];
	SetInventoryItemCounts(counts);

	return true;
#else
	if ( m_bLocalPlayerDead )
		return false;

	CGameObject* localPlayer = GetPlayerBySlot(m_localPlayerSlot);
	if ( !localPlayer )
		return false;

	CInventoryComponent* inventory = GetLocalPlayerInventory();
	if ( !inventory )
		return false;

	const CInventoryComponent::EUseResult result = inventory->UseItemSlot(slot);
	if ( result == CInventoryComponent::EUseResult::Failed )
		return false;

	SpawnInventoryUseBurst(localPlayer, slot);

	if ( result == CInventoryComponent::EUseResult::AttackPowerStateChanged )
	{
		RefreshPlayerWeaponAttackPowersForSlot(m_localPlayerSlot);
		inventory->ConsumeAttackPowerDirty();
	}

	SyncLocalInventoryToHud();

	return true;
#endif
}
