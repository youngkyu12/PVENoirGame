//-----------------------------------------------------------------------------
// File: GameSceneHUD.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameSceneHUD.h"

#include "Camera.h"
#include "GlobalValues.h"

#include <algorithm>

void CGameSceneHUD::ReleaseResources()
{
	m_ui.ReleaseResources();

	m_hpFrameSpriteIndex = -1;

	m_pauseSpriteIndex = -1;
	m_resumeSpriteIndex = -1;
	m_exitSpriteIndex = -1;
	m_poisonOverlaySpriteIndex = -1;

	m_hpFillSpriteIndex = -1;
	m_bossHpEmptySpriteIndex = -1;
	m_bossHpFillSpriteIndex = -1;
	m_bossHpFillOriginalRect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_bossHealthRatio = 1.0f;
	m_bossHealthVisible = false;

	m_otherPlayerHpEmptySpriteIndices.fill(-1);
	m_otherPlayerHpFillSpriteIndices.fill(-1);
	for ( auto& nameSpriteIndices : m_otherPlayerHpNameSpriteIndices ) nameSpriteIndices.fill(-1);
	m_otherPlayerHpSlotByGauge.fill(-1);
	for ( XMFLOAT4& rect : m_otherPlayerHpOriginalRects ) rect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	for ( XMFLOAT4& rect : m_otherPlayerHpNameOriginalRects ) rect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	m_inventorySpriteIndices.fill(-1);
	m_inventoryIconSpriteIndices.fill(-1);
	m_inventoryCooldownSpriteIndices.fill(-1);
	for ( XMFLOAT4& rect : m_inventoryCooldownOriginalRects ) rect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_inventoryItemCounts.fill(0);
	m_inventoryCountGlyphSpriteIndices.fill(-1);
	m_inventoryCooldownRatios.fill(0.0f);
	m_hpFillOriginalRect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f); 
	m_healthRatio = 1.0f;

	m_inactiveOverlayVisible = false;
}

void CGameSceneHUD::BuildResources(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	ID3D12RootSignature* rootSignature)
{
	m_ui.BuildShader(dev, cmd, rootSignature);

	m_pauseSpriteIndex = -1;
	m_resumeSpriteIndex = -1;
	m_exitSpriteIndex = -1;
	m_poisonOverlaySpriteIndex = -1;
	m_bossHpEmptySpriteIndex = -1;
	m_bossHpFillSpriteIndex = -1;
	m_bossHpFillOriginalRect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_bossHealthRatio = 1.0f;
	m_bossHealthVisible = false;

	m_otherPlayerHpEmptySpriteIndices.fill(-1);
	m_otherPlayerHpFillSpriteIndices.fill(-1);
	for ( auto& nameSpriteIndices : m_otherPlayerHpNameSpriteIndices ) nameSpriteIndices.fill(-1);
	m_otherPlayerHpSlotByGauge.fill(-1);
	for ( XMFLOAT4& rect : m_otherPlayerHpOriginalRects ) rect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	for ( XMFLOAT4& rect : m_otherPlayerHpNameOriginalRects ) rect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	m_inventorySpriteIndices.fill(-1);
	m_inventoryIconSpriteIndices.fill(-1);
	m_inventoryCooldownSpriteIndices.fill(-1);
	for ( XMFLOAT4& rect : m_inventoryCooldownOriginalRects ) rect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_inventoryCountGlyphSpriteIndices.fill(-1);

	// --------------------------------------------------------------------
	// UI layout tuning block
	// - rect = (centerX, centerY, width, height)
	// --------------------------------------------------------------------
	const HudLayout layout = CalculateLayout();

	// --------------------------------------------------------------------
	// Frame layer
	// --------------------------------------------------------------------
	m_hpFrameSpriteIndex = m_ui.AddSprite(
		dev,
		cmd,
		"HPFrame",
		L"Assets/UI/low_darkness_bar.dds",
		layout.hpFrameRect,
		CSceneUI::ELayer::Frame,
		true
	);

	// --------------------------------------------------------------------
	// Content layer
	// --------------------------------------------------------------------
	m_hpFillOriginalRect = layout.hpFillRect;

	m_hpFillSpriteIndex = m_ui.AddSprite(
        dev,
        cmd,
        "HPFill",
        L"Assets/UI/HP.dds",
        m_hpFillOriginalRect,
        CSceneUI::ELayer::Content,
        true
  );

	m_poisonOverlaySpriteIndex = m_ui.AddSolidRect("PoisonEdgeOverlay", CSceneUI::GetFullscreenRect(), CSceneUI::ELayer::Background, false, XMFLOAT4(0.10f, 0.95f, 0.18f, 0.0f), 4, XMFLOAT4(kPoisonOverlayThicknessPx, 0.0f, 0.0f, 0.0f));

	// --------------------------------------------------------------------
	// Other player HP gauges
	// - rect = (centerX, centerY, width, height)
	// - 로컬 플레이어 HP 게이지 아래에 로컬 플레이어를 제외한 3명의 이름 + HP를 표시한다.
	// - 이름은 게이지 좌측 끝에 맞춰 게이지 위에 표시한다.
	// - EmptyHP -> HP -> Name 순서로 추가한다.
	// --------------------------------------------------------------------
	
	const wchar_t* otherHpNameTexturePaths[4] = { L"Assets/UI/Player0txt.dds", L"Assets/UI/Player1txt.dds", L"Assets/UI/Player2txt.dds", L"Assets/UI/Player3txt.dds" };

	for ( int i = 0; i < kOtherPlayerHpGaugeCount; ++i )
	{
		const XMFLOAT4 gaugeRect = layout.otherPlayerHpGaugeRects[i];
		const XMFLOAT4 nameRect = layout.otherPlayerHpNameRects[i];

		m_otherPlayerHpOriginalRects[i] = gaugeRect;
		m_otherPlayerHpNameOriginalRects[i] = nameRect;

		char emptySpriteName[64] = {};
		sprintf_s(emptySpriteName, "OtherPlayerEmptyHP_%d", i);

		char fillSpriteName[64] = {};
		sprintf_s(fillSpriteName, "OtherPlayerHP_%d", i);

		m_otherPlayerHpEmptySpriteIndices[i] = m_ui.AddSprite(dev, cmd, emptySpriteName, L"Assets/UI/EmptyHP.dds", gaugeRect, CSceneUI::ELayer::Content, true);
		m_otherPlayerHpFillSpriteIndices[i] = m_ui.AddSprite(dev, cmd, fillSpriteName, L"Assets/UI/HP.dds", gaugeRect, CSceneUI::ELayer::Content, true);

		for ( int slot = 0; slot < 4; ++slot )
		{
			char nameSpriteName[64] = {};
			sprintf_s(nameSpriteName, "OtherPlayerName_%d_%d", i, slot);
			m_otherPlayerHpNameSpriteIndices[i][slot] = m_ui.AddSprite(dev, cmd, nameSpriteName, otherHpNameTexturePaths[slot], nameRect, CSceneUI::ELayer::Content, false);
		}
	}

	// --------------------------------------------------------------------
	// Inventory layer
	// rect = (centerX, centerY, width, height)
	// - 우측 하단 기준 세로 정렬.
	// - 슬롯 순서는 위에서부터 0, 1, 2, 3.
	// --------------------------------------------------------------------
	const char* inventorySpriteNames[kInventorySlotCount] = { "InventorySlot0", "InventorySlot1", "InventorySlot2", "InventorySlot3" };
	const char* inventoryIconSpriteNames[kInventorySlotCount] = { "InventoryPotionHeal", "InventoryPotionAttackUp", "InventoryPotionDefenceUp", "InventoryPotionSpeedUp" };
	const wchar_t* inventoryIconTexturePaths[kInventorySlotCount] = { L"Assets/UI/Potion_Heal.dds", L"Assets/UI/Potion_AttackUP.dds", L"Assets/UI/Potion_DefenseUP.dds", L"Assets/UI/Potion_SpeedUP.dds" };

	for ( int i = 0; i < kInventorySlotCount; ++i )
	{
		m_inventorySpriteIndices[i] = m_ui.AddSprite(
			dev, cmd, inventorySpriteNames[i], L"Assets/UI/Inventory.dds",
			layout.inventorySlotRects[i], CSceneUI::ELayer::Frame, true
		);
		m_inventoryIconSpriteIndices[i] = m_ui.AddSprite(
			dev, cmd, inventoryIconSpriteNames[i], inventoryIconTexturePaths[i],
			layout.inventoryIconRects[i], CSceneUI::ELayer::Content, true
		);
	}

	for ( int slot = 0; slot < kInventorySlotCount; ++slot )
	{
		char cooldownSpriteName[64] = {};
		sprintf_s(cooldownSpriteName, "InventoryCooldown_%d", slot);

		m_inventoryCooldownOriginalRects[slot] = layout.inventoryCooldownRects[slot];
		m_inventoryCooldownSpriteIndices[slot] = m_ui.AddSolidRect(cooldownSpriteName, m_inventoryCooldownOriginalRects[slot], CSceneUI::ELayer::Content, false);
	}

	const wchar_t* inventoryCountGlyphTexturePaths[kInventoryCountGlyphTypeCount] = { L"Assets/UI/Text_x.dds", L"Assets/UI/Text_0.dds", L"Assets/UI/Text_1.dds", L"Assets/UI/Text_2.dds", L"Assets/UI/Text_3.dds", L"Assets/UI/Text_4.dds", L"Assets/UI/Text_5.dds", L"Assets/UI/Text_6.dds", L"Assets/UI/Text_7.dds", L"Assets/UI/Text_8.dds", L"Assets/UI/Text_9.dds" };

	for ( int slot = 0; slot < kInventorySlotCount; ++slot )
	{
		for ( int charIndex = 0; charIndex < kInventoryCountTextMaxChars; ++charIndex )
		{
			for ( int glyphIndex = 0; glyphIndex < kInventoryCountGlyphTypeCount; ++glyphIndex )
			{
				char spriteName[64] = {};
				sprintf_s(spriteName, "InventoryCount_%d_%d_%d", slot, charIndex, glyphIndex);

				const int flatIndex = InventoryCountGlyphFlatIndex(slot, charIndex, glyphIndex);

				m_inventoryCountGlyphSpriteIndices[flatIndex] = m_ui.AddSprite(dev, cmd, spriteName, inventoryCountGlyphTexturePaths[glyphIndex], XMFLOAT4(0.0f, 0.0f, 36.0f, 42.0f), CSceneUI::ELayer::Content, false);

				if ( m_inventoryCountGlyphSpriteIndices[flatIndex] >= 0 )
					m_ui.SetSpriteEffectKind(m_inventoryCountGlyphSpriteIndices[flatIndex], 2);
			}
		}
	}

	for ( int slot = 0; slot < kInventorySlotCount; ++slot )
		UpdateInventoryCountTextSprites(slot);

	// --------------------------------------------------------------------
	// Boss HP gauge
	// - 화면 하단 중앙에 표시한다.
	// - 우측 하단 세로 아이템 슬롯과 겹치지 않도록 폭을 제한한다.
	// - EmptyHP를 먼저 그리고 HP를 그 위에 그린다.
	// --------------------------------------------------------------------
	m_bossHpEmptySpriteIndex = m_ui.AddSprite(
		dev, cmd,
		"BossEmptyHP",
		L"Assets/UI/EmptyHP.dds",
		layout.bossHpRect,
		CSceneUI::ELayer::Content,
		false
	);

	m_bossHpFillOriginalRect = layout.bossHpRect;

	m_bossHpFillSpriteIndex = m_ui.AddSprite(
		dev, cmd,
		"BossHP",
		L"Assets/UI/HP.dds",
		layout.bossHpRect,
		CSceneUI::ELayer::Content,
		false
	);

	m_pauseSpriteIndex = m_ui.AddSprite(
		dev,
		cmd,
		"Pause",
		L"Assets/UI/Pause.dds",
		layout.pauseRect,
		CSceneUI::ELayer::Pause,
		true
	);

	m_resumeSpriteIndex = m_ui.AddSprite(
		dev,
		cmd,
		"Resume",
		L"Assets/UI/Resume.dds",
		layout.resumeRect,
		CSceneUI::ELayer::Pause,
		true
	);

	m_exitSpriteIndex = m_ui.AddSprite(
		dev,
		cmd,
		"Exit",
		L"Assets/UI/Exit.dds",
		layout.exitRect,
		CSceneUI::ELayer::Pause,
		true
	);

	m_ui.SetLayerVisible(CSceneUI::ELayer::Pause, m_inactiveOverlayVisible);
}

void CGameSceneHUD::SetHealthRatio(float ratio)
{
	m_healthRatio = std::clamp(ratio, 0.0f, 1.0f);

	if ( m_hpFillSpriteIndex < 0 )
		return;

	const float originalCenterX = m_hpFillOriginalRect.x;
	const float originalCenterY = m_hpFillOriginalRect.y;
	const float originalWidth = m_hpFillOriginalRect.z;
	const float originalHeight = m_hpFillOriginalRect.w;

	const float newWidth = originalWidth * m_healthRatio;

	// 왼쪽 고정, 오른쪽만 줄어드는 방식.
	const float leftX = originalCenterX - originalWidth * 0.5f;
	const float newCenterX = leftX + newWidth * 0.5f;

	m_ui.SetSpriteRect(
		m_hpFillSpriteIndex,
		XMFLOAT4(newCenterX, originalCenterY, newWidth, originalHeight)
	);
}

void CGameSceneHUD::SetBossHealthRatio(float ratio, bool visible)
{
	m_bossHealthRatio = std::clamp(ratio, 0.0f, 1.0f);
	m_bossHealthVisible = visible;

	if ( m_bossHpEmptySpriteIndex >= 0 )
		m_ui.SetSpriteVisible(m_bossHpEmptySpriteIndex, visible);

	if ( !visible || m_bossHpFillSpriteIndex < 0 )
	{
		if ( m_bossHpFillSpriteIndex >= 0 )
			m_ui.SetSpriteVisible(m_bossHpFillSpriteIndex, false);

		return;
	}

	const float clampedRatio = std::clamp(ratio, 0.0f, 1.0f);
	const float originalCenterX = m_bossHpFillOriginalRect.x;
	const float originalCenterY = m_bossHpFillOriginalRect.y;
	const float originalWidth = m_bossHpFillOriginalRect.z;
	const float originalHeight = m_bossHpFillOriginalRect.w;

	if ( clampedRatio <= 0.001f || originalWidth <= 0.0f || originalHeight <= 0.0f )
	{
		m_ui.SetSpriteVisible(m_bossHpFillSpriteIndex, false);
		return;
	}

	const float newWidth = originalWidth * clampedRatio;
	const float leftX = originalCenterX - originalWidth * 0.5f;
	const float newCenterX = leftX + newWidth * 0.5f;

	m_ui.SetSpriteRect(m_bossHpFillSpriteIndex, XMFLOAT4(newCenterX, originalCenterY, newWidth, originalHeight));
	m_ui.SetSpriteVisible(m_bossHpFillSpriteIndex, true);
}

void CGameSceneHUD::SetOtherPlayerHealthRatios(int localPlayerSlot, const std::array<float, 4>& playerHpRatios, const std::array<bool, 4>& playerHpVisible, const std::array<bool, 4>& playerWorldHpGaugeVisible)
{
	int gaugeIndex = 0;

	for ( int slot = 0; slot < 4; ++slot )
	{
		if ( slot == localPlayerSlot )
			continue;

		if ( playerWorldHpGaugeVisible[slot] )
			continue;

		if ( gaugeIndex >= kOtherPlayerHpGaugeCount )
			break;

		const int emptySpriteIndex = m_otherPlayerHpEmptySpriteIndices[gaugeIndex];
		const int fillSpriteIndex = m_otherPlayerHpFillSpriteIndices[gaugeIndex];

		for ( int nameSlot = 0; nameSlot < 4; ++nameSlot )
		{
			const int nameSpriteIndex = m_otherPlayerHpNameSpriteIndices[gaugeIndex][nameSlot];
			if ( nameSpriteIndex >= 0 )
				m_ui.SetSpriteVisible(nameSpriteIndex, false);
		}

		m_otherPlayerHpSlotByGauge[gaugeIndex] = slot;

		const bool visible = playerHpVisible[slot];

		if ( emptySpriteIndex >= 0 )
			m_ui.SetSpriteVisible(emptySpriteIndex, visible);

		if ( fillSpriteIndex >= 0 )
			m_ui.SetSpriteVisible(fillSpriteIndex, visible);

		if ( slot >= 0 && slot < 4 )
		{
			const int nameSpriteIndex = m_otherPlayerHpNameSpriteIndices[gaugeIndex][slot];
			if ( nameSpriteIndex >= 0 )
			{
				m_ui.SetSpriteRect(nameSpriteIndex, m_otherPlayerHpNameOriginalRects[gaugeIndex]);
				m_ui.SetSpriteVisible(nameSpriteIndex, visible);
			}
		}

		if ( visible && fillSpriteIndex >= 0 )
		{
			const float ratio = std::clamp(playerHpRatios[slot], 0.0f, 1.0f);
			const XMFLOAT4 originalRect = m_otherPlayerHpOriginalRects[gaugeIndex];

			if ( ratio <= 0.001f || originalRect.z <= 0.0f || originalRect.w <= 0.0f )
			{
				m_ui.SetSpriteVisible(fillSpriteIndex, false);
			}
			else
			{
				const float newWidth = originalRect.z * ratio;
				const float leftX = originalRect.x - originalRect.z * 0.5f;
				const float newCenterX = leftX + newWidth * 0.5f;

				m_ui.SetSpriteRect(fillSpriteIndex, XMFLOAT4(newCenterX, originalRect.y, newWidth, originalRect.w));
				m_ui.SetSpriteVisible(fillSpriteIndex, true);
			}
		}

		++gaugeIndex;
	}

	for ( ; gaugeIndex < kOtherPlayerHpGaugeCount; ++gaugeIndex )
	{
		m_otherPlayerHpSlotByGauge[gaugeIndex] = -1;

		if ( m_otherPlayerHpEmptySpriteIndices[gaugeIndex] >= 0 )
			m_ui.SetSpriteVisible(m_otherPlayerHpEmptySpriteIndices[gaugeIndex], false);

		if ( m_otherPlayerHpFillSpriteIndices[gaugeIndex] >= 0 )
			m_ui.SetSpriteVisible(m_otherPlayerHpFillSpriteIndices[gaugeIndex], false);

		for ( int nameSlot = 0; nameSlot < 4; ++nameSlot )
		{
			const int nameSpriteIndex = m_otherPlayerHpNameSpriteIndices[gaugeIndex][nameSlot];
			if ( nameSpriteIndex >= 0 )
				m_ui.SetSpriteVisible(nameSpriteIndex, false);
		}
	}
}

void CGameSceneHUD::SetInventoryItemCounts(const std::array<int, kInventorySlotCount>& counts)
{
	for ( int i = 0; i < kInventorySlotCount; ++i )
	{
		m_inventoryItemCounts[i] = counts[i] < 0 ? 0 : counts[i];

		const bool empty = ( m_inventoryItemCounts[i] <= 0 );

		if ( m_inventoryIconSpriteIndices[i] >= 0 )
		{
			m_ui.SetSpriteVisible(m_inventoryIconSpriteIndices[i], true);
			m_ui.SetSpriteEffectKind(m_inventoryIconSpriteIndices[i], empty ? 1 : 0);
		}

		UpdateInventoryCountTextSprites(i);
	}
}

void CGameSceneHUD::SetInventoryCooldownRatio(int slot, float ratio)
{
	if ( slot < 0 || slot >= kInventorySlotCount )
		return;

	if ( ratio < 0.0f )
		ratio = 0.0f;

	if ( ratio > 1.0f )
		ratio = 1.0f;

	m_inventoryCooldownRatios[slot] = ratio;

	const int spriteIndex = m_inventoryCooldownSpriteIndices[slot];

	if ( spriteIndex < 0 )
		return;

	const XMFLOAT4 originalRect = m_inventoryCooldownOriginalRects[slot];

	if ( ratio <= 0.001f || originalRect.z <= 0.0f || originalRect.w <= 0.0f )
	{
		m_ui.SetSpriteVisible(spriteIndex, false);
		return;
	}

	const float newHeight = originalRect.w * ratio;
	const float bottomY = originalRect.y + originalRect.w * 0.5f;
	const float newCenterY = bottomY - newHeight * 0.5f;

	m_ui.SetSpriteRect(spriteIndex, XMFLOAT4(originalRect.x, newCenterY, originalRect.z, newHeight));
	m_ui.SetSpriteVisible(spriteIndex, true);
}

void CGameSceneHUD::UpdateInventoryCountTextSprites(int slot)
{
	if ( slot < 0 || slot >= kInventorySlotCount )
		return;

	for ( int charIndex = 0; charIndex < kInventoryCountTextMaxChars; ++charIndex )
	{
		for ( int glyphIndex = 0; glyphIndex < kInventoryCountGlyphTypeCount; ++glyphIndex )
		{
			const int flatIndex = InventoryCountGlyphFlatIndex(slot, charIndex, glyphIndex);

			const int textSpriteIndex = m_inventoryCountGlyphSpriteIndices[flatIndex];
			if ( textSpriteIndex >= 0 )
				m_ui.SetSpriteVisible(textSpriteIndex, false);
		}
	}

	XMFLOAT4 slotRect{};
	if ( !m_ui.GetSpriteRect(m_inventorySpriteIndices[slot], slotRect) )
		return;

	int displayCount = m_inventoryItemCounts[slot];

	if ( displayCount < 0 )
		displayCount = 0;

	if ( displayCount > 999 )
		displayCount = 999;

	std::string text = "x" + std::to_string(displayCount);

	if ( static_cast< int >( text.size() ) > kInventoryCountTextMaxChars )
		text = text.substr(0, kInventoryCountTextMaxChars);

	const float charWidth = 24.0f;
	const float charHeight = 28.0f;
	const float charAdvance = 8.0f;
	const float rightPadding = -5.0f;
	const float bottomPadding = -5.0f;

	const int charCount = static_cast< int >( text.size() );
	const float totalWidth = charWidth + charAdvance * static_cast< float >( charCount - 1 );
	const float rightEdge = slotRect.x + slotRect.z * 0.5f - rightPadding;
	const float bottomEdge = slotRect.y + slotRect.w * 0.5f - bottomPadding;
	const float firstCenterX = rightEdge - totalWidth + charWidth * 0.5f;
	const float centerY = bottomEdge - charHeight * 0.5f;

	for ( int charIndex = 0; charIndex < charCount && charIndex < kInventoryCountTextMaxChars; ++charIndex )
	{
		const char ch = text[static_cast< size_t >(charIndex)];

		int glyphIndex = -1;

		if ( ch == 'x' || ch == 'X' )
			glyphIndex = 0;
		else if ( ch >= '0' && ch <= '9' )
			glyphIndex = 1 + ( ch - '0' );

		if ( glyphIndex < 0 || glyphIndex >= kInventoryCountGlyphTypeCount )
			continue;

		const int flatIndex = InventoryCountGlyphFlatIndex(slot, charIndex, glyphIndex);
		const float centerX = firstCenterX + charAdvance * static_cast< float >(charIndex);

		const int textSpriteIndex = m_inventoryCountGlyphSpriteIndices[flatIndex];
		if ( textSpriteIndex >= 0 )
		{
			m_ui.SetSpriteRect(textSpriteIndex, XMFLOAT4(centerX, centerY, charWidth, charHeight));
			m_ui.SetSpriteVisible(textSpriteIndex, true);
		}
	}
}

void CGameSceneHUD::SetPoisonOverlayAlpha(float alpha)
{
	alpha = std::clamp(alpha, 0.0f, 1.0f);

	if ( m_poisonOverlaySpriteIndex < 0 )
		return;

	if ( alpha <= 0.001f )
	{
		m_ui.SetSpriteVisible(m_poisonOverlaySpriteIndex, false);
		return;
	}

	const float finalAlpha = alpha * kPoisonOverlayMaxAlpha;

	m_ui.SetSpriteColor(m_poisonOverlaySpriteIndex, XMFLOAT4(0.10f, 0.95f, 0.18f, finalAlpha));
	m_ui.SetSpriteParams0(m_poisonOverlaySpriteIndex, XMFLOAT4(kPoisonOverlayThicknessPx, 0.0f, 0.0f, 0.0f));
	m_ui.SetSpriteVisible(m_poisonOverlaySpriteIndex, true);
}

void CGameSceneHUD::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	m_ui.SetLayerVisible(CSceneUI::ELayer::Pause, m_inactiveOverlayVisible);
	m_ui.RenderAll(cmd, camera);
}

void CGameSceneHUD::SetInactiveOverlayVisible(bool visible)
{
	m_inactiveOverlayVisible = visible;
	m_ui.SetLayerVisible(CSceneUI::ELayer::Pause, visible);
}

bool CGameSceneHUD::GetPauseOverlayRect(XMFLOAT4& outRect) const
{
	return m_ui.GetSpriteRect(m_pauseSpriteIndex, outRect);
}

bool CGameSceneHUD::GetResumeButtonRect(XMFLOAT4& outRect) const
{
	return m_ui.GetSpriteRect(m_resumeSpriteIndex, outRect);
}

bool CGameSceneHUD::GetExitButtonRect(XMFLOAT4& outRect) const
{
	return m_ui.GetSpriteRect(m_exitSpriteIndex, outRect);
}

bool CGameSceneHUD::IsPointInPauseOverlay(POINT clientPt) const
{
	return m_ui.IsPointInSprite(m_pauseSpriteIndex, clientPt);
}

bool CGameSceneHUD::IsPointInResumeButton(POINT clientPt) const
{
	return m_ui.IsPointInSprite(m_resumeSpriteIndex, clientPt);
}

bool CGameSceneHUD::IsPointInExitButton(POINT clientPt) const
{
	return m_ui.IsPointInSprite(m_exitSpriteIndex, clientPt);
}

void CGameSceneHUD::OnResize(int width, int height)
{
	if (width <= 0 || height <= 0)
		return;

	m_screenWidth = static_cast<float>(width);
	m_screenHeight = static_cast<float>(height);

	m_ui.OnResize(width, height);

	const HudLayout layout = CalculateLayout();

	m_ui.SetSpriteRect(m_hpFrameSpriteIndex, layout.hpFrameRect);

	m_hpFillOriginalRect = layout.hpFillRect;
	SetHealthRatio(m_healthRatio);

	for (int i = 0; i < kInventorySlotCount; ++i)
	{
		m_ui.SetSpriteRect(m_inventorySpriteIndices[i], layout.inventorySlotRects[i]);
		m_ui.SetSpriteRect(m_inventoryIconSpriteIndices[i], layout.inventoryIconRects[i]);

		m_inventoryCooldownOriginalRects[i] = layout.inventoryCooldownRects[i];
		SetInventoryCooldownRatio(i, m_inventoryCooldownRatios[i]);

		UpdateInventoryCountTextSprites(i);
	}

	m_ui.SetSpriteRect(m_pauseSpriteIndex, layout.pauseRect);
	m_ui.SetSpriteRect(m_resumeSpriteIndex, layout.resumeRect);
	m_ui.SetSpriteRect(m_exitSpriteIndex, layout.exitRect);

	m_ui.SetSpriteRect(m_bossHpEmptySpriteIndex, layout.bossHpRect);
	m_ui.SetSpriteRect(m_bossHpFillSpriteIndex, layout.bossHpRect);

	m_bossHpFillOriginalRect = layout.bossHpRect;
	SetBossHealthRatio(m_bossHealthRatio, m_bossHealthVisible);

	for ( int i = 0; i < kOtherPlayerHpGaugeCount; ++i )
	{
		m_otherPlayerHpOriginalRects[i] = layout.otherPlayerHpGaugeRects[i];
		m_otherPlayerHpNameOriginalRects[i] = layout.otherPlayerHpNameRects[i];

		m_ui.SetSpriteRect(m_otherPlayerHpEmptySpriteIndices[i], layout.otherPlayerHpGaugeRects[i]);
		m_ui.SetSpriteRect(m_otherPlayerHpFillSpriteIndices[i], layout.otherPlayerHpGaugeRects[i]);

		for ( int slot = 0; slot < 4; ++slot )
		{
			m_ui.SetSpriteRect(m_otherPlayerHpNameSpriteIndices[i][slot], layout.otherPlayerHpNameRects[i]);
		}
	}
}

CGameSceneHUD::HudLayout CGameSceneHUD::CalculateLayout() const
{
	HudLayout layout{};

	const float screenW = m_screenWidth;
	const float screenH = m_screenHeight;

	const float hpFrameMarginLeft = 12.0f;
	const float hpFrameMarginTop = 12.0f;

	const float hpFrameWidth =
		std::clamp(screenW * 0.18f, 260.0f, 380.0f);

	const float hpFrameHeight =
		std::clamp(screenH * 0.035f, 34.0f, 48.0f);

	const float hpFrameCenterX =
		hpFrameMarginLeft + hpFrameWidth * 0.5f;

	const float hpFrameCenterY =
		hpFrameMarginTop + hpFrameHeight * 0.5f;

	layout.hpFrameRect =
		XMFLOAT4(hpFrameCenterX, hpFrameCenterY, hpFrameWidth, hpFrameHeight);

	layout.hpFillRect =
		XMFLOAT4(hpFrameCenterX, hpFrameCenterY, hpFrameWidth - 10.0f, hpFrameHeight - 6.0f);

	const float inventorySlotSize =
		std::clamp(screenH * 0.075f, 56.0f, 84.0f);

	const float inventoryRightMargin = 10.0f;
	const float inventoryBottomMargin = 10.0f;
	const float inventoryTotalWidth =
		inventorySlotSize * static_cast<float>(kInventorySlotCount);

	const float inventoryStartCenterX =
		screenW - inventoryRightMargin - inventoryTotalWidth + inventorySlotSize * 0.5f;

	const float inventoryCenterY =
		screenH - inventoryBottomMargin - inventorySlotSize * 0.5f;

	const float inventoryIconSize = inventorySlotSize * 0.92f;

	for (int i = 0; i < kInventorySlotCount; ++i)
	{
		const float centerX =
			inventoryStartCenterX + inventorySlotSize * static_cast<float>(i);

		layout.inventorySlotRects[i] =
			XMFLOAT4(centerX, inventoryCenterY, inventorySlotSize, inventorySlotSize);

		layout.inventoryIconRects[i] =
			XMFLOAT4(centerX, inventoryCenterY, inventoryIconSize, inventoryIconSize);

		layout.inventoryCooldownRects[i] =
			layout.inventorySlotRects[i];
	}

	layout.pauseRect =
		XMFLOAT4(screenW * 0.5f, screenH * 0.5f, screenW, screenH);

	layout.resumeRect =
		XMFLOAT4(screenW * 0.5f, screenH * 0.43f, screenW * 0.40f, screenH * 0.16f);

	layout.exitRect =
		XMFLOAT4(screenW * 0.5f, screenH * 0.66f, screenW * 0.40f, screenH * 0.16f);

	const float bossHpWidth = std::clamp(screenW * 0.32f, 320.0f, 520.0f);
	const float bossHpHeight = std::clamp(screenH * 0.018f, 14.0f, 22.0f);
	const float bossHpBottomMargin = std::clamp(screenH * 0.045f, 28.0f, 48.0f);

	layout.bossHpRect =	XMFLOAT4(screenW * 0.5f, screenH - bossHpBottomMargin, bossHpWidth, bossHpHeight);

	const float otherHpGaugeWidth = 115.0f;
	const float otherHpGaugeHeight = 10.0f;
	const float otherHpNameWidth = 68.0f;
	const float otherHpNameHeight = 30.0f;
	const float otherHpNameToGaugeGap = -2.0f;
	const float otherHpGaugeVerticalGap = 38.0f;
	const float otherHpGroupTopMargin = 6.0f;
	const float otherHpLeftX = layout.hpFillRect.x - layout.hpFillRect.z * 0.5f;
	const float otherHpGaugeCenterX = otherHpLeftX + otherHpGaugeWidth * 0.5f;
	const float otherHpNameCenterX = otherHpLeftX + otherHpNameWidth * 0.5f;
	const float otherHpFirstNameCenterY = 
		layout.hpFrameRect.y + layout.hpFrameRect.w * 0.5f
		+ otherHpGroupTopMargin
		+ otherHpNameHeight * 0.5f;
	const float otherHpGaugeStartCenterY = otherHpFirstNameCenterY + otherHpNameHeight * 0.5f + otherHpNameToGaugeGap + otherHpGaugeHeight * 0.5f;

	for ( int i = 0; i < kOtherPlayerHpGaugeCount; ++i )
	{
		const float gaugeCenterY =
			otherHpGaugeStartCenterY + otherHpGaugeVerticalGap * static_cast<float>(i);

		const float nameCenterY =
			otherHpFirstNameCenterY + otherHpGaugeVerticalGap * static_cast<float>(i);

		layout.otherPlayerHpGaugeRects[i] =
			XMFLOAT4(otherHpGaugeCenterX, gaugeCenterY, otherHpGaugeWidth, otherHpGaugeHeight);

		layout.otherPlayerHpNameRects[i] =
			XMFLOAT4(otherHpNameCenterX, nameCenterY, otherHpNameWidth, otherHpNameHeight);
	}

	return layout;
}
