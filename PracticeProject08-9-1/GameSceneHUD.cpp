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

	m_pauseSpriteIndex = -1;
	m_resumeSpriteIndex = -1;
	m_exitSpriteIndex = -1;

	m_hpFillSpriteIndex = -1;

	m_otherPlayerHpEmptySpriteIndices.fill(-1);
	m_otherPlayerHpFillSpriteIndices.fill(-1);
	m_otherPlayerHpSlotByGauge.fill(-1);
	for ( XMFLOAT4& rect : m_otherPlayerHpOriginalRects ) rect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	m_inventorySpriteIndices.fill(-1);
	m_inventoryIconSpriteIndices.fill(-1);
	m_inventoryCooldownSpriteIndices.fill(-1);
	for ( XMFLOAT4& rect : m_inventoryCooldownOriginalRects ) rect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_inventoryItemCounts.fill(0);
	m_inventoryCountGlyphSpriteIndices.fill(-1);
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

	m_otherPlayerHpEmptySpriteIndices.fill(-1);
	m_otherPlayerHpFillSpriteIndices.fill(-1);
	m_otherPlayerHpSlotByGauge.fill(-1);
	for ( XMFLOAT4& rect : m_otherPlayerHpOriginalRects ) rect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	m_inventorySpriteIndices.fill(-1);
	m_inventoryIconSpriteIndices.fill(-1);
	m_inventoryCooldownSpriteIndices.fill(-1);
	for ( XMFLOAT4& rect : m_inventoryCooldownOriginalRects ) rect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_inventoryCountGlyphSpriteIndices.fill(-1);

	// --------------------------------------------------------------------
	// UI layout tuning block
	// - rect = (centerX, centerY, width, height)
	// --------------------------------------------------------------------
	const float hpFrameCenterX = 150.0f;
	const float hpFrameCenterY = 20.0f;
	const float hpFrameWidth = 300.0f;
	const float hpFrameHeight = 40.0f;

	const float hpBarCenterX = hpFrameCenterX;
	const float hpBarCenterY = hpFrameCenterY;
	const float hpBarWidth = 290.0f;
	const float hpBarHeight = 34.0f;

	// --------------------------------------------------------------------
	// Frame layer
	// --------------------------------------------------------------------
	m_ui.AddSprite(
		dev,
		cmd,
		"HPFrame",
		L"Assets/UI/low_darkness_bar.dds",
		XMFLOAT4(hpFrameCenterX, hpFrameCenterY, hpFrameWidth, hpFrameHeight),
		CSceneUI::ELayer::Frame,
		true
	);

	// --------------------------------------------------------------------
	// Content layer
	// --------------------------------------------------------------------
	m_hpFillOriginalRect = XMFLOAT4(hpBarCenterX, hpBarCenterY, hpBarWidth, hpBarHeight);

	m_hpFillSpriteIndex = m_ui.AddSprite(
		dev,
		cmd,
		"HPFill",
		L"Assets/UI/HP.dds",
		m_hpFillOriginalRect,
		CSceneUI::ELayer::Content,
		true
	);

	// --------------------------------------------------------------------
	// Pause layer
	// rect = (centerX, centerY, width, height)
	// --------------------------------------------------------------------
	const float screenW = static_cast< float >( FRAME_BUFFER_WIDTH );
	const float screenH = static_cast< float >( FRAME_BUFFER_HEIGHT );

	// --------------------------------------------------------------------
	// Other player HP gauges
	// - rect = (centerX, centerY, width, height)
	// - 로컬 플레이어 HP 게이지 바로 아래에 로컬 플레이어를 제외한 3명의 HP를 표시한다.
	// - EmptyHP를 먼저 추가하고, HP를 그 뒤에 추가해서 같은 레이어 안에서 EmptyHP -> HP 순서로 렌더되게 한다.
	// --------------------------------------------------------------------
	const float otherHpGaugeWidth = 115.0f;
	const float otherHpGaugeHeight = 10.0f;
	const float otherHpGaugeVerticalGap = 24.0f;
	const float otherHpGaugeTopMargin = 18.0f;
	const float otherHpGaugeCenterX = hpBarCenterX - hpBarWidth * 0.5f + otherHpGaugeWidth * 0.5f;
	const float otherHpGaugeStartCenterY = hpFrameCenterY + hpFrameHeight * 0.5f + otherHpGaugeTopMargin + otherHpGaugeHeight * 0.5f;

	for ( int i = 0; i < kOtherPlayerHpGaugeCount; ++i )
	{
		const float centerY = otherHpGaugeStartCenterY + otherHpGaugeVerticalGap * static_cast< float >(i);
		const XMFLOAT4 gaugeRect(otherHpGaugeCenterX, centerY, otherHpGaugeWidth, otherHpGaugeHeight);

		m_otherPlayerHpOriginalRects[i] = gaugeRect;

		char emptySpriteName[64] = {};
		sprintf_s(emptySpriteName, "OtherPlayerEmptyHP_%d", i);

		char fillSpriteName[64] = {};
		sprintf_s(fillSpriteName, "OtherPlayerHP_%d", i);

		m_otherPlayerHpEmptySpriteIndices[i] = m_ui.AddSprite(dev, cmd, emptySpriteName, L"Assets/UI/EmptyHP.dds", gaugeRect, CSceneUI::ELayer::Content, true);
		m_otherPlayerHpFillSpriteIndices[i] = m_ui.AddSprite(dev, cmd, fillSpriteName, L"Assets/UI/HP.dds", gaugeRect, CSceneUI::ELayer::Content, true);
	}

	// --------------------------------------------------------------------
	// Inventory layer
	// rect = (centerX, centerY, width, height)
	// --------------------------------------------------------------------
	const float inventorySlotWidth = 72.0f;
	const float inventorySlotHeight = 72.0f;
	const float inventoryRightMargin = 10.0f;
	const float inventoryBottomMargin = 10.0f;
	const float inventoryTotalWidth = inventorySlotWidth * static_cast< float >( kInventorySlotCount );
	const float inventoryStartCenterX = screenW - inventoryRightMargin - inventoryTotalWidth + inventorySlotWidth * 0.5f;
	const float inventoryCenterY = screenH - inventoryBottomMargin - inventorySlotHeight * 0.5f;
	const float inventoryIconSize = inventorySlotWidth * 0.92f;
	const char* inventorySpriteNames[kInventorySlotCount] = { "InventorySlot0", "InventorySlot1", "InventorySlot2", "InventorySlot3" };
	const char* inventoryIconSpriteNames[kInventorySlotCount] = { "InventoryPotionHeal", "InventoryPotionAttackUp", "InventoryPotionDefenceUp", "InventoryPotionSpeedUp" };
	const wchar_t* inventoryIconTexturePaths[kInventorySlotCount] = { L"Assets/UI/Potion_Heal.dds", L"Assets/UI/Potion_AttackUP.dds", L"Assets/UI/Potion_DefenseUP.dds", L"Assets/UI/Potion_SpeedUP.dds" };

	for ( int i = 0; i < kInventorySlotCount; ++i )
	{
		const float centerX = inventoryStartCenterX + inventorySlotWidth * static_cast< float >(i);
		m_inventorySpriteIndices[i] = m_ui.AddSprite(dev, cmd, inventorySpriteNames[i], L"Assets/UI/Inventory.dds", XMFLOAT4(centerX, inventoryCenterY, inventorySlotWidth, inventorySlotHeight), CSceneUI::ELayer::Frame, true);
		m_inventoryIconSpriteIndices[i] = m_ui.AddSprite(dev, cmd, inventoryIconSpriteNames[i], inventoryIconTexturePaths[i], XMFLOAT4(centerX, inventoryCenterY, inventoryIconSize, inventoryIconSize), CSceneUI::ELayer::Content, true);
	}

	for ( int slot = 0; slot < kInventorySlotCount; ++slot )
	{
		const float centerX = inventoryStartCenterX + inventorySlotWidth * static_cast< float >(slot);
		char cooldownSpriteName[64] = {};
		sprintf_s(cooldownSpriteName, "InventoryCooldown_%d", slot);

		m_inventoryCooldownOriginalRects[slot] = XMFLOAT4(centerX, inventoryCenterY, inventorySlotWidth, inventorySlotHeight);
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

	const XMFLOAT4 pauseRect(
		screenW * 0.5f,
		screenH * 0.5f,
		screenW,
		screenH
	);

	const XMFLOAT4 resumeRect(
		screenW * 0.5f,
		screenH * 0.43f,
		screenW * 0.40f,
		screenH * 0.16f
	);

	const XMFLOAT4 exitRect(
		screenW * 0.5f,
		screenH * 0.66f,
		screenW * 0.40f,
		screenH * 0.16f
	);

	m_pauseSpriteIndex = m_ui.AddSprite(
		dev,
		cmd,
		"Pause",
		L"Assets/UI/Pause.dds",
		pauseRect,
		CSceneUI::ELayer::Pause,
		true
	);

	m_resumeSpriteIndex = m_ui.AddSprite(
		dev,
		cmd,
		"Resume",
		L"Assets/UI/Resume.dds",
		resumeRect,
		CSceneUI::ELayer::Pause,
		true
	);

	m_exitSpriteIndex = m_ui.AddSprite(
		dev,
		cmd,
		"Exit",
		L"Assets/UI/Exit.dds",
		exitRect,
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

		m_otherPlayerHpSlotByGauge[gaugeIndex] = slot;

		const bool visible = playerHpVisible[slot];

		if ( emptySpriteIndex >= 0 )
			m_ui.SetSpriteVisible(emptySpriteIndex, visible);

		if ( fillSpriteIndex >= 0 )
			m_ui.SetSpriteVisible(fillSpriteIndex, visible);

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

	const float charWidth = 36.0f;
	const float charHeight = 42.0f;
	const float charAdvance = 13.0f;
	const float rightPadding = -8.0f;
	const float bottomPadding = -8.0f;

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