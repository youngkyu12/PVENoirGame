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
	m_inventorySpriteIndices.fill(-1);
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
	m_inventorySpriteIndices.fill(-1);

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
	// Inventory layer
	// rect = (centerX, centerY, width, height)
	// --------------------------------------------------------------------
	const float inventorySlotWidth = 60.0f;
	const float inventorySlotHeight = 60.0f;
	const float inventoryRightMargin = 10.0f;
	const float inventoryBottomMargin = 10.0f;
	const float inventoryTotalWidth = inventorySlotWidth * static_cast< float >( kInventorySlotCount );
	const float inventoryStartCenterX = screenW - inventoryRightMargin - inventoryTotalWidth + inventorySlotWidth * 0.5f;
	const float inventoryCenterY = screenH - inventoryBottomMargin - inventorySlotHeight * 0.5f;
	const char* inventorySpriteNames[kInventorySlotCount] = { "InventorySlot0", "InventorySlot1", "InventorySlot2", "InventorySlot3", "InventorySlot4" };

	for ( int i = 0; i < kInventorySlotCount; ++i )
	{
		const float centerX = inventoryStartCenterX + inventorySlotWidth * static_cast< float >(i);
		m_inventorySpriteIndices[i] = m_ui.AddSprite(dev, cmd, inventorySpriteNames[i], L"Assets/UI/Inventory.dds", XMFLOAT4(centerX, inventoryCenterY, inventorySlotWidth, inventorySlotHeight), CSceneUI::ELayer::Frame, true);
	}

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