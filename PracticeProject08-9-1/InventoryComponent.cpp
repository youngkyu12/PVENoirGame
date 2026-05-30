//-----------------------------------------------------------------------------
// File: InventoryComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "InventoryComponent.h"
#include "HealthComponent.h"
#include "PlayerControllerComponent.h"
#include "Object.h"

namespace
{
	float Clamp01(float value)
	{
		if ( value < 0.0f )
			return 0.0f;

		if ( value > 1.0f )
			return 1.0f;

		return value;
	}
}

CInventoryComponent::CInventoryComponent(CGameObject* owner)
	: CComponentT(owner)
{
}

void CInventoryComponent::SetItemCounts(const std::array<int, kInventorySlotCount>& counts)
{
	for ( int i = 0; i < kInventorySlotCount; ++i )
		m_itemCounts[i] = counts[i] < 0 ? 0 : counts[i];
}

void CInventoryComponent::AddItemCount(int slot, int amount)
{
	if ( slot < 0 || slot >= kInventorySlotCount )
		return;

	if ( amount <= 0 )
		return;

	m_itemCounts[static_cast< size_t >(slot)] += amount;
}

bool CInventoryComponent::HasItem(int slot) const
{
	if ( slot < 0 || slot >= kInventorySlotCount )
		return false;

	return m_itemCounts[static_cast< size_t >(slot)] > 0;
}

CInventoryComponent::EUseResult CInventoryComponent::UseItemSlot(int slot)
{
	if ( slot < 0 || slot >= kInventorySlotCount )
		return EUseResult::Failed;

	if ( m_itemCounts[static_cast< size_t >(slot)] <= 0 )
		return EUseResult::Failed;

	bool itemEffectApplied = false;
	bool attackPowerStateChanged = false;

	switch ( slot )
	{
	case 0:
		itemEffectApplied = TryUseHealPotion();
		break;

	case 1:
		itemEffectApplied = TryBeginAttackPowerPotion();
		attackPowerStateChanged = itemEffectApplied;
		break;

	case 2:
		itemEffectApplied = TryBeginDefensePotion();
		break;

	case 3:
		itemEffectApplied = TryBeginMoveSpeedPotion();
		break;

	default:
		return EUseResult::Failed;
	}

	if ( !itemEffectApplied )
		return EUseResult::Failed;

	--m_itemCounts[static_cast< size_t >( slot )];

	return attackPowerStateChanged ? EUseResult::AttackPowerStateChanged : EUseResult::Used;
}

void CInventoryComponent::OnUpdate(float dt)
{
	if ( dt < 0.0f )
		dt = 0.0f;

	UpdateMoveSpeedPotion(dt);
	UpdateAttackPowerPotion(dt);
	UpdateDefensePotion(dt);
}

void CInventoryComponent::OnDestroy()
{
	ForceRestoreAllEffects();
}

float CInventoryComponent::GetCooldownRatio(int slot) const
{
	if ( slot < 0 || slot >= kInventorySlotCount )
		return 0.0f;

	switch ( slot )
	{
	case 1:
		return m_bAttackPowerPotionActive ? Clamp01(m_attackPowerPotionRemainingSec / kAttackPowerPotionDurationSec) : 0.0f;

	case 2:
		return m_bDefensePotionActive ? Clamp01(m_defensePotionRemainingSec / kDefensePotionDurationSec) : 0.0f;

	case 3:
		return m_bMoveSpeedPotionActive ? Clamp01(m_moveSpeedPotionRemainingSec / kMoveSpeedPotionDurationSec) : 0.0f;

	default:
		return 0.0f;
	}
}

int CInventoryComponent::ApplyAttackPowerMultiplier(int attackPower) const
{
	if ( m_bAttackPowerPotionActive )
		return attackPower * kAttackPowerPotionMultiplier;

	return attackPower;
}

bool CInventoryComponent::ConsumeAttackPowerDirty()
{
	const bool dirty = m_bAttackPowerDirty;
	m_bAttackPowerDirty = false;
	return dirty;
}

void CInventoryComponent::ForceRestoreAllEffects()
{
	RestoreMoveSpeedPotion();
	RestoreAttackPowerPotion();
	RestoreDefensePotion();
}

bool CInventoryComponent::TryUseHealPotion()
{
	if ( !m_pOwner )
		return false;

	CHealthComponent* hp = m_pOwner->GetComponent<CHealthComponent>();
	if ( !hp )
		return false;

	if ( hp->IsDead() )
		return false;

	if ( hp->GetCurrentHp() >= hp->GetMaxHp() )
		return false;

	const int hpBefore = hp->GetCurrentHp();
	hp->Heal(kHealPotionRecoverAmount);

	return hp->GetCurrentHp() > hpBefore;
}

bool CInventoryComponent::TryBeginMoveSpeedPotion()
{
	if ( m_bMoveSpeedPotionActive )
	{
		return false;
	}

	if ( !m_pOwner )
		return false;

	CHealthComponent* hp = m_pOwner->GetComponent<CHealthComponent>();
	if ( hp && hp->IsDead() )
		return false;

	CPlayerControllerComponent* pc = m_pOwner->GetComponent<CPlayerControllerComponent>();
	if ( !pc )
		return false;

	m_moveSpeedPotionOriginalWalkSpeed = pc->GetWalkMoveSpeed();
	m_moveSpeedPotionOriginalRunSpeed = pc->GetRunMoveSpeed();

	pc->SetMoveSpeeds(kMoveSpeedPotionWalkSpeed, kMoveSpeedPotionRunSpeed);

	m_bMoveSpeedPotionActive = true;
	m_moveSpeedPotionRemainingSec = kMoveSpeedPotionDurationSec;
	m_moveSpeedPotionLastLoggedSecond = static_cast< int >( kMoveSpeedPotionDurationSec );

	return true;
}

void CInventoryComponent::UpdateMoveSpeedPotion(float dt)
{
	if ( !m_bMoveSpeedPotionActive )
		return;

	m_moveSpeedPotionRemainingSec -= dt;

	if ( m_moveSpeedPotionRemainingSec <= 0.0f )
	{
		RestoreMoveSpeedPotion();
		return;
	}

	const int remainingSecond = static_cast< int >( m_moveSpeedPotionRemainingSec + 0.999f );

	if ( remainingSecond != m_moveSpeedPotionLastLoggedSecond )
	{
		m_moveSpeedPotionLastLoggedSecond = remainingSecond;
	}
}

void CInventoryComponent::RestoreMoveSpeedPotion()
{
	if ( !m_bMoveSpeedPotionActive )
		return;

	if ( m_pOwner )
	{
		CPlayerControllerComponent* pc = m_pOwner->GetComponent<CPlayerControllerComponent>();
		if ( pc )
			pc->SetMoveSpeeds(m_moveSpeedPotionOriginalWalkSpeed, m_moveSpeedPotionOriginalRunSpeed);
	}

	m_bMoveSpeedPotionActive = false;
	m_moveSpeedPotionRemainingSec = 0.0f;
	m_moveSpeedPotionLastLoggedSecond = -1;
}

bool CInventoryComponent::TryBeginAttackPowerPotion()
{
	if ( m_bAttackPowerPotionActive )
	{
		return false;
	}

	if ( !m_pOwner )
		return false;

	CHealthComponent* hp = m_pOwner->GetComponent<CHealthComponent>();
	if ( hp && hp->IsDead() )
		return false;

	m_bAttackPowerPotionActive = true;
	m_attackPowerPotionRemainingSec = kAttackPowerPotionDurationSec;
	m_attackPowerPotionLastLoggedSecond = static_cast< int >( kAttackPowerPotionDurationSec );
	m_bAttackPowerDirty = true;

	return true;
}

void CInventoryComponent::UpdateAttackPowerPotion(float dt)
{
	if ( !m_bAttackPowerPotionActive )
		return;

	m_attackPowerPotionRemainingSec -= dt;

	if ( m_attackPowerPotionRemainingSec <= 0.0f )
	{
		RestoreAttackPowerPotion();
		return;
	}

	const int remainingSecond = static_cast< int >( m_attackPowerPotionRemainingSec + 0.999f );

	if ( remainingSecond != m_attackPowerPotionLastLoggedSecond )
	{
		m_attackPowerPotionLastLoggedSecond = remainingSecond;
	}
}

void CInventoryComponent::RestoreAttackPowerPotion()
{
	if ( !m_bAttackPowerPotionActive )
		return;

	m_bAttackPowerPotionActive = false;
	m_attackPowerPotionRemainingSec = 0.0f;
	m_attackPowerPotionLastLoggedSecond = -1;
	m_bAttackPowerDirty = true;
}

bool CInventoryComponent::TryBeginDefensePotion()
{
	if ( m_bDefensePotionActive )
	{
		return false;
	}

	if ( !m_pOwner )
		return false;

	CHealthComponent* hp = m_pOwner->GetComponent<CHealthComponent>();
	if ( !hp )
		return false;

	if ( hp->IsDead() )
		return false;

	m_defensePotionOriginalIncomingDamageScale = hp->GetIncomingDamageScale();
	hp->SetIncomingDamageScale(kDefensePotionIncomingDamageScale);

	m_bDefensePotionActive = true;
	m_defensePotionRemainingSec = kDefensePotionDurationSec;
	m_defensePotionLastLoggedSecond = static_cast< int >( kDefensePotionDurationSec );

	return true;
}

void CInventoryComponent::UpdateDefensePotion(float dt)
{
	if ( !m_bDefensePotionActive )
		return;

	m_defensePotionRemainingSec -= dt;

	if ( m_defensePotionRemainingSec <= 0.0f )
	{
		RestoreDefensePotion();
		return;
	}

	const int remainingSecond = static_cast< int >( m_defensePotionRemainingSec + 0.999f );

	if ( remainingSecond != m_defensePotionLastLoggedSecond )
	{
		m_defensePotionLastLoggedSecond = remainingSecond;
	}
}

void CInventoryComponent::RestoreDefensePotion()
{
	if ( !m_bDefensePotionActive )
		return;

	if ( m_pOwner )
	{
		CHealthComponent* hp = m_pOwner->GetComponent<CHealthComponent>();
		if ( hp )
			hp->SetIncomingDamageScale(m_defensePotionOriginalIncomingDamageScale);
	}

	m_bDefensePotionActive = false;
	m_defensePotionRemainingSec = 0.0f;
	m_defensePotionOriginalIncomingDamageScale = 1.0f;
	m_defensePotionLastLoggedSecond = -1;
}