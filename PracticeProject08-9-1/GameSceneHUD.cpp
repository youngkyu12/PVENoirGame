//-----------------------------------------------------------------------------
// File: GameSceneHUD.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameSceneHUD.h"

#include "Camera.h"
#include "GlobalValues.h"

void CGameSceneHUD::ReleaseResources()
{
	m_ui.ReleaseResources();

	m_pauseSpriteIndex = -1;
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
	constexpr int kItemSlotCount = 5;
	constexpr int kEquipSlotCount = 2;

	const float itemFrameCenterX = FRAME_BUFFER_WIDTH - 105.0f;
	const float itemFrameCenterY = FRAME_BUFFER_HEIGHT - 22.5f;
	const float itemFrameWidth = 210.0f;
	const float itemFrameHeight = 45.0f;

	const float itemSlotSize = 32.0f;
	const float itemSlotSpacing = 37.0f;
	const float itemSlotStartX =
		itemFrameCenterX - ( ( kItemSlotCount - 1 ) * itemSlotSpacing * 0.5f );
	const float itemSlotCenterY = itemFrameCenterY;

	const float equipFrameCenterX = FRAME_BUFFER_WIDTH - 45.0f;
	const float equipFrameCenterY = FRAME_BUFFER_HEIGHT - 66.0f;
	const float equipFrameWidth = 90.0f;
	const float equipFrameHeight = 45.0f;

	const float equipSlotSize = 32.0f;
	const float equipSlotSpacing = 37.0f;
	const float equipSlotStartX =
		equipFrameCenterX - ( ( kEquipSlotCount - 1 ) * equipSlotSpacing * 0.5f );
	const float equipSlotCenterY = equipFrameCenterY;

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
		"ItemFrame",
		L"Assets/UI/low_darkness_bar.dds",
		XMFLOAT4(itemFrameCenterX, itemFrameCenterY, itemFrameWidth, itemFrameHeight),
		CSceneUI::ELayer::Frame,
		true
	);

	m_ui.AddSprite(
		dev,
		cmd,
		"EquipmentFrame",
		L"Assets/UI/low_darkness_bar.dds",
		XMFLOAT4(equipFrameCenterX, equipFrameCenterY, equipFrameWidth, equipFrameHeight),
		CSceneUI::ELayer::Frame,
		true
	);

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
	for ( int i = 0; i < kItemSlotCount; ++i )
	{
		const float centerX = itemSlotStartX + ( itemSlotSpacing * i );

		char name[64] = {};
		sprintf_s(name, "ItemSlot_%d", i);

		m_ui.AddSprite(
			dev,
			cmd,
			name,
			L"Assets/UI/mini_dark_bar1.dds",
			XMFLOAT4(centerX, itemSlotCenterY, itemSlotSize, itemSlotSize),
			CSceneUI::ELayer::Content,
			true
		);
	}

	for ( int i = 0; i < kEquipSlotCount; ++i )
	{
		const float centerX = equipSlotStartX + ( equipSlotSpacing * i );

		char name[64] = {};
		sprintf_s(name, "EquipmentSlot_%d", i);

		m_ui.AddSprite(
			dev,
			cmd,
			name,
			L"Assets/UI/mini_dark_bar1.dds",
			XMFLOAT4(centerX, equipSlotCenterY, equipSlotSize, equipSlotSize),
			CSceneUI::ELayer::Content,
			true
		);
	}

	m_ui.AddSprite(
		dev,
		cmd,
		"HPFill",
		L"Assets/UI/HP.dds",
		XMFLOAT4(hpBarCenterX, hpBarCenterY, hpBarWidth, hpBarHeight),
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