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
	m_hpFillSpriteIndex = -1;
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
	// --------------------------------------------------------------------
	m_pauseSpriteIndex = m_ui.AddFitSprite(
		dev,
		cmd,
		"Pause",
		L"Assets/UI/Pause.dds",
		FRAME_BUFFER_WIDTH * 0.5f,
		FRAME_BUFFER_HEIGHT * 0.5f,
		static_cast< float >( FRAME_BUFFER_WIDTH ),
		static_cast< float >( FRAME_BUFFER_HEIGHT ),
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

bool CGameSceneHUD::IsPointInPauseOverlay(POINT clientPt) const
{
	return m_ui.IsPointInSprite(m_pauseSpriteIndex, clientPt);
}