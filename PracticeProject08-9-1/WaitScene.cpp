//-----------------------------------------------------------------------------
// File: WaitScene.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "WaitScene.h"

#include "Object.h"
#include "Camera.h"
#include "AudioManager.h"
#include "MusicDirector.h"
#include "GlobalValues.h"
#include "Service.h"
#include "ServerPacketHandler.h"
#include "GlobalValues.h"

XMFLOAT4 CWaitScene::GetStartButtonRect() const
{
	const float buttonW = static_cast< float >( m_viewportWidth ) * 0.40f;
	const float buttonH = static_cast< float >( m_viewportHeight ) * 0.18f;
	const float marginX = static_cast< float >( m_viewportWidth ) * 0.04f;
	const float marginY = static_cast< float >( m_viewportHeight ) * 0.04f;
	const float centerX = static_cast< float >( m_viewportWidth ) - marginX - buttonW * 0.5f;
	const float centerY = static_cast< float >( m_viewportHeight ) - marginY - buttonH * 0.5f;
	return XMFLOAT4(centerX, centerY, buttonW, buttonH);
}

XMFLOAT4 CWaitScene::GetWeaponGridRect() const
{
	const float left = static_cast< float >( m_viewportWidth ) * 0.08f;
	const float top = static_cast< float >( m_viewportHeight ) * 0.13f;
	const float right = static_cast< float >( m_viewportWidth ) * 0.92f;
	const float bottom = static_cast< float >( m_viewportHeight ) * 0.76f;
	const float width = right - left;
	const float height = bottom - top;
	return XMFLOAT4(left + width * 0.5f, top + height * 0.5f, width, height);
}

XMFLOAT4 CWaitScene::GetWeaponFrameRect(int frameSlot) const
{
	const int safeSlot = std::clamp(frameSlot, 0, 3);

	const float left = static_cast< float >( m_viewportWidth ) * 0.08f;
	const float right = static_cast< float >( m_viewportWidth ) * 0.88f;
	const float usableW = right - left;
	const float cellW = usableW / 4.0f;

	const float frameW = cellW * 0.95f;
	const float frameH = frameW * 2.0f;

	const float baseY = static_cast< float >( m_viewportHeight ) * 0.50f;
	const float yOffset = static_cast< float >( m_viewportHeight ) * 0.045f;

	const float groupCenterX = ( left + right ) * 0.5f;
	const float spacingX = cellW * 1.1f;
	const float centerX = groupCenterX + ( static_cast< float >( safeSlot ) - 1.5f ) * spacingX;
	const float centerY = baseY + ( ( safeSlot % 2 == 0 ) ? -yOffset : yOffset );

	return XMFLOAT4(centerX, centerY, frameW, frameH);
}

XMFLOAT4 CWaitScene::GetWeaponSpriteRect(int frameSlot) const
{
	const XMFLOAT4 frameRect = GetWeaponFrameRect(frameSlot);
	constexpr float kWeaponScale = 0.8f;
	return XMFLOAT4(frameRect.x, frameRect.y, frameRect.z * kWeaponScale, frameRect.w * kWeaponScale);
}

XMFLOAT4 CWaitScene::GetWeaponSelectedRect(int frameSlot) const
{
	const XMFLOAT4 frameRect = GetWeaponFrameRect(frameSlot);
	constexpr float kSelectedScaleX = 1.7f;
	constexpr float kSelectedScaleY = 1.3f;
	return XMFLOAT4(frameRect.x, frameRect.y, frameRect.z * kSelectedScaleX, frameRect.w * kSelectedScaleY);
}

XMFLOAT4 CWaitScene::GetPlayerMarkerRect(int playerIndex, int weaponSlot) const
{
	const int safePlayerIndex = std::clamp(playerIndex, 0, 3);
	const int safeWeaponSlot = std::clamp(weaponSlot, 0, 3);
	const XMFLOAT4 frameRect = GetWeaponFrameRect(safeWeaponSlot);

	const float markerW = frameRect.z * 0.52f;
	const float markerH = markerW * 0.55f;
	const float insetX = frameRect.z * -0.1f;
	const float gapY = frameRect.w * -0.05f;

	const float frameLeft = frameRect.x - frameRect.z * 0.5f;
	const float frameRight = frameRect.x + frameRect.z * 0.5f;
	const float frameTop = frameRect.y - frameRect.w * 0.5f;
	const float frameBottom = frameRect.y + frameRect.w * 0.5f;

	const bool isLeft = ( safePlayerIndex == 0 || safePlayerIndex == 2 );
	const bool isTop = ( safePlayerIndex == 0 || safePlayerIndex == 1 );

	const float centerX = isLeft ? frameLeft + insetX + markerW * 0.5f : frameRight - insetX - markerW * 0.5f;
	const float centerY = isTop ? frameTop - gapY - markerH * 0.5f : frameBottom + gapY + markerH * 0.5f;

	return XMFLOAT4(centerX, centerY, markerW, markerH);
}

void CWaitScene::SetLocalPlayerIndex(int playerIndex)
{
	m_localPlayerIndex = std::clamp(playerIndex, 0, 3);
}

void CWaitScene::SetPlayerWeaponSelection(int playerIndex, int weaponSlot)
{
	const int safePlayerIndex = std::clamp(playerIndex, 0, 3);
	const int safeWeaponSlot = std::clamp(weaponSlot, 0, 3);

	m_playerSelectedWeaponSlots[safePlayerIndex] = safeWeaponSlot;
	m_playerWeaponSelectionKnown[safePlayerIndex] = true;

	g_waitSceneSelectedWeaponSlots[safePlayerIndex] = safeWeaponSlot;
	g_waitSceneWeaponSelectionKnown[safePlayerIndex] = true;

	UpdatePlayerMarkerSpriteRect(safePlayerIndex);
}

void CWaitScene::UpdatePlayerMarkerSpriteRect(int playerIndex)
{
	const int safePlayerIndex = std::clamp(playerIndex, 0, 3);

	if ( !m_playerWeaponSelectionKnown[safePlayerIndex] ) return;

	const int weaponSlot = m_playerSelectedWeaponSlots[safePlayerIndex];
	if ( weaponSlot < 0 || weaponSlot >= 4 ) return;

	if ( const auto* marker = m_waitUI.GetSprite(m_playerMarkerSpriteIndices[safePlayerIndex]) )
	{
		const XMFLOAT4 markerRect = GetPlayerMarkerRect(safePlayerIndex, weaponSlot);
		m_waitUI.SetSpriteRect(m_playerMarkerSpriteIndices[safePlayerIndex], CSceneUI::MakeFitRect(marker->texture, markerRect.x, markerRect.y, markerRect.z, markerRect.w));
	}
}

int CWaitScene::GetWeaponSlotAtPoint(POINT ptClient) const
{
	for ( int i = 0; i < 4; ++i )
	{
		const XMFLOAT4 rect = GetWeaponFrameRect(i);
		const float left = rect.x - rect.z * 0.5f;
		const float right = rect.x + rect.z * 0.5f;
		const float top = rect.y - rect.w * 0.5f;
		const float bottom = rect.y + rect.w * 0.5f;

		if ( static_cast< float >(ptClient.x) >= left && static_cast< float >(ptClient.x) <= right && static_cast< float >(ptClient.y) >= top && static_cast< float >(ptClient.y) <= bottom ) return i;
	}

	return -1;
}

void CWaitScene::UpdateHoveredWeaponSlot(POINT ptClient)
{
	if ( m_showLoading )
	{
		m_hoveredWeaponSlot = -1;
		return;
	}

	const int previousHoveredWeaponSlot = m_hoveredWeaponSlot;
	const int nextHoveredWeaponSlot = GetWeaponSlotAtPoint(ptClient);

	m_hoveredWeaponSlot = nextHoveredWeaponSlot;

	if ( nextHoveredWeaponSlot >= 0 && nextHoveredWeaponSlot != previousHoveredWeaponSlot )
	{
		if ( m_pAudioManager ) m_pAudioManager->PlaySound2D("Assets/Audio/Weaponselecting.wav", false, false, 0.1f, false);
	}
}

void CWaitScene::BuildObjects(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
	CreateGraphicsRootSignature(dev);
	CreateMainCamera(dev, cmd, nullptr);
	m_waitUI.BuildShader(dev, cmd, GetGraphicsRootSignature());

#ifdef USING_NETWORK
	m_localPlayerIndex = ( g_myPlayerId <= 3 ) ? static_cast< int >( g_myPlayerId ) : 0;
#else
	m_localPlayerIndex = 0;
#endif

	m_playerSelectedWeaponSlots = { -1, -1, -1, -1 };
	m_playerWeaponSelectionKnown = { false, false, false, false };

	g_waitSceneSelectedWeaponSlots = { -1, -1, -1, -1 };
	g_waitSceneWeaponSelectionKnown = { false, false, false, false };

	m_playerSelectedWeaponSlots[m_localPlayerIndex] = 0;
	m_playerWeaponSelectionKnown[m_localPlayerIndex] = true;
	m_isReady = false;

	g_waitSceneSelectedWeaponSlots[m_localPlayerIndex] = 0;
	g_waitSceneWeaponSelectionKnown[m_localPlayerIndex] = true;

	m_waitBackgroundSpriteIndex = m_waitUI.AddSprite(dev, cmd, "WaitBackground", L"Assets/UI/WaitSceneImage.dds", CSceneUI::GetFullscreenRect(m_viewportWidth, m_viewportHeight), CSceneUI::ELayer::Background, true);

	for ( int i = 0; i < 4; ++i )
	{
		const XMFLOAT4 selectedRect = GetWeaponSelectedRect(i);
		m_weaponSelectedSpriteIndices[i] = m_waitUI.AddSprite(dev, cmd, "WeaponSelected", L"Assets/UI/SelectedWeapons.dds", selectedRect, CSceneUI::ELayer::Content, false);
	}

	for ( int i = 0; i < 4; ++i )
	{
		const XMFLOAT4 frameRect = GetWeaponFrameRect(i);
		m_weaponFrameSpriteIndices[i] = m_waitUI.AddFitSprite(dev, cmd, "WeaponFrame", L"Assets/UI/Frame.dds", frameRect.x, frameRect.y, frameRect.z, frameRect.w, CSceneUI::ELayer::Content, true);
	}

	static constexpr const wchar_t* kWeaponTexturePaths[4] = { L"Assets/UI/Sword.dds", L"Assets/UI/Bow.dds", L"Assets/UI/Axe.dds", L"Assets/UI/Gun.dds" };
	static constexpr const char* kWeaponSpriteNames[4] = { "WeaponSword", "WeaponBow", "WeaponAxe", "WeaponGun" };

	for ( int i = 0; i < 4; ++i )
	{
		const XMFLOAT4 weaponRect = GetWeaponSpriteRect(i);
		m_weaponSpriteIndices[i] = m_waitUI.AddFitSprite(dev, cmd, kWeaponSpriteNames[i], kWeaponTexturePaths[i], weaponRect.x, weaponRect.y, weaponRect.z, weaponRect.w, CSceneUI::ELayer::Content, true);
	}

	static constexpr const wchar_t* kPlayerMarkerTexturePaths[4] = { L"Assets/UI/1P.dds", L"Assets/UI/2P.dds", L"Assets/UI/3P.dds", L"Assets/UI/4P.dds" };
	static constexpr const char* kPlayerMarkerSpriteNames[4] = { "PlayerMarker1P", "PlayerMarker2P", "PlayerMarker3P", "PlayerMarker4P" };

	for ( int i = 0; i < 4; ++i )
	{
		const int weaponSlot = m_playerWeaponSelectionKnown[i] ? m_playerSelectedWeaponSlots[i] : 0;
		const XMFLOAT4 markerRect = GetPlayerMarkerRect(i, weaponSlot);
		m_playerMarkerSpriteIndices[i] = m_waitUI.AddFitSprite(dev, cmd, kPlayerMarkerSpriteNames[i], kPlayerMarkerTexturePaths[i], markerRect.x, markerRect.y, markerRect.z, markerRect.w, CSceneUI::ELayer::Content, m_playerWeaponSelectionKnown[i]);
	}

	const XMFLOAT4 startRect = GetStartButtonRect();
	m_startButtonSpriteIndex = m_waitUI.AddFitSprite(dev, cmd, "WaitStartButton", L"Assets/UI/ReadyButton.dds", startRect.x, startRect.y, startRect.z, startRect.w, CSceneUI::ELayer::Content, true);
	m_loadingSpriteIndex = m_waitUI.AddSprite(dev, cmd, "WaitLoading", L"Assets/UI/LoadingImage.dds", CSceneUI::GetFullscreenRect(m_viewportWidth, m_viewportHeight), CSceneUI::ELayer::Content, false);

	if ( m_pAudioManager )
	{
		if ( auto* music = m_pAudioManager->GetMusicDirector() )
		{
			m_pAudioManager->SetGroupVolume(m_pAudioManager->GetBgmGroup(), 0.15f);

			music->SetCrossFadeSeconds(0.35f);
			music->RequestState(EMusicState::Wait, false);
			music->BeginPendingTransition();
		}
	}
}

void CWaitScene::ReleaseObjects()
{
	m_waitUI.ReleaseResources();
	m_waitBackgroundSpriteIndex = -1;
	m_weaponSelectedSpriteIndices = { -1, -1, -1, -1 };
	m_weaponFrameSpriteIndices = { -1, -1, -1, -1 };
	m_weaponSpriteIndices = { -1, -1, -1, -1 };
	m_playerMarkerSpriteIndices = { -1, -1, -1, -1 };
	m_playerSelectedWeaponSlots = { -1, -1, -1, -1 };
	m_playerWeaponSelectionKnown = { false, false, false, false };
	m_localPlayerIndex = 0;
	m_hoveredWeaponSlot = -1;
	m_startButtonSpriteIndex = -1;
	m_loadingSpriteIndex = -1;
	m_isReady = false;
	CScene::ReleaseObjects();
}

void CWaitScene::CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* /*target*/)
{
	m_pMainCameraObject = std::make_unique<CGameObject>(0);

	auto* cam = m_pMainCameraObject->AddComponent<CThirdPersonCamera>();
	m_pMainCamera = cam;

	cam->SetMode(THIRD_PERSON_CAMERA);
	cam->SetTarget(nullptr);
	cam->SetTimeLag(0.0f);
	cam->SetOffset(XMFLOAT3(0.0f, 0.0f, 0.0f));

	const float aspectRatio = static_cast< float >( m_viewportWidth ) / static_cast< float >( m_viewportHeight );
	cam->GenerateProjectionMatrix(1.01f, 5000.0f, aspectRatio, 60.0f);
	cam->SetViewport(0, 0, m_viewportWidth, m_viewportHeight, 0.0f, 1.0f);
	cam->SetScissorRect(0, 0, m_viewportWidth, m_viewportHeight);

	m_pMainCameraObject->CreateComponents(dev, cmd);

	XMFLOAT3 pos(0.0f, 2.0f, -5.0f);
	XMFLOAT3 lookAt(0.0f, 0.5f, 0.0f);
	XMFLOAT3 up(0.0f, 1.0f, 0.0f);

	cam->SetPosition(pos);
	cam->SetLookAtPosition(lookAt);
	cam->GenerateViewMatrix(pos, lookAt, up);
}

void CWaitScene::OnPrepareRender(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	CScene::OnPrepareRender(cmd, camera);
}

void CWaitScene::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) camera = m_pMainCamera;

	for ( int i = 0; i < 4; ++i )
	{
		const bool showWeaponUi = !m_showLoading;
		m_waitUI.SetSpriteVisible(m_weaponSelectedSpriteIndices[i], showWeaponUi && ( m_hoveredWeaponSlot == i ));
		m_waitUI.SetSpriteVisible(m_weaponFrameSpriteIndices[i], showWeaponUi);
		m_waitUI.SetSpriteVisible(m_weaponSpriteIndices[i], showWeaponUi);
		m_waitUI.SetSpriteVisible(m_playerMarkerSpriteIndices[i], showWeaponUi && m_playerWeaponSelectionKnown[i]);
	}

	m_waitUI.SetSpriteVisible(m_startButtonSpriteIndex, !m_showLoading);
	m_waitUI.SetSpriteVisible(m_loadingSpriteIndex, m_showLoading);
	m_waitUI.RenderAll(cmd, camera);
}

bool CWaitScene::OnProcessingMouseMessage(HWND /*hWnd*/, UINT nMessageID, WPARAM /*wParam*/, LPARAM lParam)
{
	POINT ptClient{};
	ptClient.x = GET_X_LPARAM(lParam);
	ptClient.y = GET_Y_LPARAM(lParam);

	if ( nMessageID == WM_MOUSEMOVE )
	{
		UpdateHoveredWeaponSlot(ptClient);
		return false;
	}

	if ( nMessageID != WM_LBUTTONDOWN ) return false;

	UpdateHoveredWeaponSlot(ptClient);

	if ( m_showLoading ) return true;

	const int clickedWeaponSlot = GetWeaponSlotAtPoint(ptClient);
	if ( clickedWeaponSlot >= 0 )
	{
		SetPlayerWeaponSelection(m_localPlayerIndex, clickedWeaponSlot);
		if ( m_pAudioManager ) m_pAudioManager->PlaySound2D("Assets/Audio/Weaponselect.wav", false, false, 0.5f, false);
		return true;
	}

	if ( !m_waitUI.IsPointInSprite(m_startButtonSpriteIndex, ptClient) ) return false;

	if ( m_pAudioManager ) m_pAudioManager->PlaySound2D("Assets/Audio/StartEffect.mp3", false, false, 0.5f, false);

	m_isReady = true;

#ifdef USING_NETWORK
	Protocol::C_GAME_START startPkt;
	startPkt.set_playerid(g_myPlayerId);
	startPkt.set_playerweapon(0x1010);
	startPkt.set_ready(m_isReady);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(startPkt);
	g_clientService->BroadCast(sendBuffer);
#endif

	m_showLoading = true;
	m_hoveredWeaponSlot = -1;
	m_startGameRequested = true;
	return true;
}

bool CWaitScene::ConsumeSceneRequest(ESceneRequest& outReq)
{
	if ( !m_startGameRequested )
	{
		outReq = ESceneRequest::None;
		return false;
	}

	m_startGameRequested = false;
	outReq = ESceneRequest::SwitchToGame;
	return true;
}

void CWaitScene::OnResize(int width, int height)
{
	m_waitUI.OnResize(width, height);

	m_viewportWidth = width;
	m_viewportHeight = height;

	m_waitUI.SetSpriteRect(m_waitBackgroundSpriteIndex, CSceneUI::GetFullscreenRect(m_viewportWidth, m_viewportHeight));

	for ( int i = 0; i < 4; ++i )
	{
		const XMFLOAT4 selectedRect = GetWeaponSelectedRect(i);
		const XMFLOAT4 frameRect = GetWeaponFrameRect(i);

		m_waitUI.SetSpriteRect(m_weaponSelectedSpriteIndices[i], selectedRect);
		if ( const auto* frame = m_waitUI.GetSprite(m_weaponFrameSpriteIndices[i]) ) m_waitUI.SetSpriteRect(m_weaponFrameSpriteIndices[i], CSceneUI::MakeFitRect(frame->texture, frameRect.x, frameRect.y, frameRect.z, frameRect.w));

		if ( const auto* weapon = m_waitUI.GetSprite(m_weaponSpriteIndices[i]) )
		{
			const XMFLOAT4 weaponRect = GetWeaponSpriteRect(i);
			m_waitUI.SetSpriteRect(m_weaponSpriteIndices[i], CSceneUI::MakeFitRect(weapon->texture, weaponRect.x, weaponRect.y, weaponRect.z, weaponRect.w));
		}
	}

	for ( int i = 0; i < 4; ++i ) UpdatePlayerMarkerSpriteRect(i);

	if ( const auto* start = m_waitUI.GetSprite(m_startButtonSpriteIndex) )
	{
		const XMFLOAT4 startRect = GetStartButtonRect();
		m_waitUI.SetSpriteRect(m_startButtonSpriteIndex, CSceneUI::MakeFitRect(start->texture, startRect.x, startRect.y, startRect.z, startRect.w));
	}

	m_waitUI.SetSpriteRect(m_loadingSpriteIndex, CSceneUI::GetFullscreenRect(m_viewportWidth, m_viewportHeight));
}
