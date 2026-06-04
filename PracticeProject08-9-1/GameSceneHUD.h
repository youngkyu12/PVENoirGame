//-----------------------------------------------------------------------------
// File: GameSceneHUD.h
//-----------------------------------------------------------------------------

#pragma once

#include "SceneUI.h"

class CCamera;

class CGameSceneHUD final
{
public:
	static constexpr int kInventorySlotCount = 4;
	void ReleaseResources();

	void BuildResources(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		ID3D12RootSignature* rootSignature
	);

	void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera);
	
	void SetHealthRatio(float ratio);
	void SetOtherPlayerHealthRatios(int localPlayerSlot, const std::array<float, 4>& playerHpRatios, const std::array<bool, 4>& playerHpVisible, const std::array<bool, 4>& playerWorldHpGaugeVisible); 
	void SetInventoryItemCounts(const std::array<int, kInventorySlotCount>& counts);
	void SetInventoryCooldownRatio(int slot, float ratio);

	void SetInactiveOverlayVisible(bool visible);
	bool IsInactiveOverlayVisible() const { return m_inactiveOverlayVisible; }

	bool GetPauseOverlayRect(XMFLOAT4& outRect) const;
	bool GetResumeButtonRect(XMFLOAT4& outRect) const;
	bool GetExitButtonRect(XMFLOAT4& outRect) const;

	bool IsPointInPauseOverlay(POINT clientPt) const;
	bool IsPointInResumeButton(POINT clientPt) const;
	bool IsPointInExitButton(POINT clientPt) const;

private:
	CSceneUI m_ui;

	int m_pauseSpriteIndex = -1;
	int m_resumeSpriteIndex = -1;
	int m_exitSpriteIndex = -1;

	int m_hpFillSpriteIndex = -1;

	static constexpr int kOtherPlayerHpGaugeCount = 3;

	std::array<int, kOtherPlayerHpGaugeCount> m_otherPlayerHpEmptySpriteIndices = { -1, -1, -1 };
	std::array<int, kOtherPlayerHpGaugeCount> m_otherPlayerHpFillSpriteIndices = { -1, -1, -1 };
	std::array<std::array<int, 4>, kOtherPlayerHpGaugeCount> m_otherPlayerHpNameSpriteIndices = { { { -1, -1, -1, -1 }, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } } };
	std::array<XMFLOAT4, kOtherPlayerHpGaugeCount> m_otherPlayerHpOriginalRects = {};
	std::array<XMFLOAT4, kOtherPlayerHpGaugeCount> m_otherPlayerHpNameOriginalRects = {};
	std::array<int, kOtherPlayerHpGaugeCount> m_otherPlayerHpSlotByGauge = { -1, -1, -1 };

	std::array<int, kInventorySlotCount> m_inventorySpriteIndices = { -1, -1, -1, -1 };
	std::array<int, kInventorySlotCount> m_inventoryIconSpriteIndices = { -1, -1, -1, -1 };
	std::array<int, kInventorySlotCount> m_inventoryCooldownSpriteIndices = { -1, -1, -1, -1 };
	std::array<XMFLOAT4, kInventorySlotCount> m_inventoryCooldownOriginalRects = {};
	std::array<int, kInventorySlotCount> m_inventoryItemCounts = { 0, 0, 0, 0 };

	static constexpr int kInventoryCountTextMaxDigits = 3;
	static constexpr int kInventoryCountTextMaxChars = 1 + kInventoryCountTextMaxDigits;
	static constexpr int kInventoryCountGlyphTypeCount = 11;
	std::array<int, kInventorySlotCount* kInventoryCountTextMaxChars* kInventoryCountGlyphTypeCount> m_inventoryCountGlyphSpriteIndices = {};
	
	static constexpr int InventoryCountGlyphFlatIndex(int slot, int charIndex, int glyphIndex) { return ( slot * kInventoryCountTextMaxChars + charIndex ) * kInventoryCountGlyphTypeCount + glyphIndex; }
	void UpdateInventoryCountTextSprites(int slot);

	XMFLOAT4 m_hpFillOriginalRect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	float m_healthRatio = 1.0f;

	bool m_inactiveOverlayVisible = false;
};