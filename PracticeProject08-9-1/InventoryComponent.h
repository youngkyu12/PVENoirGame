//-----------------------------------------------------------------------------
// File: InventoryComponent.h
//-----------------------------------------------------------------------------

#pragma once

#include "Component.h"

#include <array>

class CInventoryComponent final : public CComponentT<CInventoryComponent>
{
public:
	static constexpr int kInventorySlotCount = 4;

	enum class EUseResult
	{
		Failed = 0,
		Used,
		AttackPowerStateChanged
	};

public:
	explicit CInventoryComponent(CGameObject* owner);

	void SetItemCounts(const std::array<int, kInventorySlotCount>& counts);
	const std::array<int, kInventorySlotCount>& GetItemCounts() const { return m_itemCounts; }

	void AddItemCount(int slot, int amount = 1);
	bool HasItem(int slot) const;

	EUseResult UseItemSlot(int slot);

	void OnUpdate(float dt) override;
	void OnDestroy() override;

	float GetCooldownRatio(int slot) const;

	bool IsMoveSpeedPotionActive() const { return m_bMoveSpeedPotionActive; }
	bool TryBeginPredictedMoveSpeedPotion();
	bool IsAttackPowerPotionActive() const { return m_bAttackPowerPotionActive; }
	bool IsDefensePotionActive() const { return m_bDefensePotionActive; }

	int ApplyAttackPowerMultiplier(int attackPower) const;

	bool ConsumeAttackPowerDirty();

	void ForceRestoreAllEffects();

private:
	bool TryUseHealPotion();
	bool TryBeginMoveSpeedPotion();
	void UpdateMoveSpeedPotion(float dt);
	void RestoreMoveSpeedPotion();

	bool TryBeginAttackPowerPotion();
	void UpdateAttackPowerPotion(float dt);
	void RestoreAttackPowerPotion();

	bool TryBeginDefensePotion();
	void UpdateDefensePotion(float dt);
	void RestoreDefensePotion();

private:
	std::array<int, kInventorySlotCount> m_itemCounts = { 0, 0, 0, 0 };

	static constexpr int kHealPotionRecoverAmount = 20;

	static constexpr float kMoveSpeedPotionDurationSec = 10.0f;
	static constexpr float kMoveSpeedPotionWalkSpeed = 10.0f;
	static constexpr float kMoveSpeedPotionRunSpeed = 20.0f;

	bool m_bMoveSpeedPotionActive = false;
	float m_moveSpeedPotionRemainingSec = 0.0f;
	float m_moveSpeedPotionOriginalWalkSpeed = 5.0f;
	float m_moveSpeedPotionOriginalRunSpeed = 10.0f;
	int m_moveSpeedPotionLastLoggedSecond = -1;

	static constexpr float kAttackPowerPotionDurationSec = 10.0f;
	static constexpr int kAttackPowerPotionMultiplier = 2;

	bool m_bAttackPowerPotionActive = false;
	float m_attackPowerPotionRemainingSec = 0.0f;
	int m_attackPowerPotionLastLoggedSecond = -1;
	bool m_bAttackPowerDirty = false;

	static constexpr float kDefensePotionDurationSec = 10.0f;
	static constexpr float kDefensePotionIncomingDamageScale = 0.5f;

	bool m_bDefensePotionActive = false;
	float m_defensePotionRemainingSec = 0.0f;
	float m_defensePotionOriginalIncomingDamageScale = 1.0f;
	int m_defensePotionLastLoggedSecond = -1;
};