#include "stdafx.h"
#include "MenuScene.h"

#include "Object.h"
#include "Camera.h"
#include "AudioManager.h"
#include "MusicDirector.h"

void CMenuScene::BuildObjects(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
	CreateGraphicsRootSignature(dev);
	CreateMainCamera(dev, cmd, nullptr);
	m_menuUI.BuildShader(dev, cmd, GetGraphicsRootSignature());

	m_menuBackgroundSpriteIndex = m_menuUI.AddSprite(dev, cmd, "MenuBackground", L"Assets/UI/MenuImage.dds", CSceneUI::GetFullscreenRect(m_viewportWidth, m_viewportHeight), CSceneUI::ELayer::Background, true);
	m_startButtonSpriteIndex = m_menuUI.AddFitSprite(dev, cmd, "StartButton", L"Assets/UI/JoinButton.dds", m_viewportWidth * 0.5f, m_viewportHeight * 0.78f, m_viewportWidth * 0.40f, m_viewportHeight * 0.18f, CSceneUI::ELayer::Content, true);

	if ( m_pAudioManager )
	{
		if ( auto* music = m_pAudioManager->GetMusicDirector() )
		{
			music->RequestState(EMusicState::Menu, true);
			music->BeginPendingTransition();
		}
	}
}

void CMenuScene::ReleaseObjects()
{
	m_menuUI.ReleaseResources();
	m_menuBackgroundSpriteIndex = -1;
	m_startButtonSpriteIndex = -1;
	CScene::ReleaseObjects();
}

void CMenuScene::CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* /*target*/)
{
	m_pMainCameraObject = std::make_unique<CGameObject>(0);

	auto* cam = m_pMainCameraObject->AddComponent<CThirdPersonCamera>();
	m_pMainCamera = cam;

	cam->SetMode(THIRD_PERSON_CAMERA);
	cam->SetTarget(nullptr);

	cam->SetTimeLag(0.0f);
	cam->SetOffset(XMFLOAT3(0.0f, 0.0f, 0.0f));

	const float aspectRatio =
		static_cast<float>(m_viewportWidth) / static_cast<float>(m_viewportHeight);

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

void CMenuScene::OnPrepareRender(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	CScene::OnPrepareRender(cmd, camera);
}

void CMenuScene::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) camera = m_pMainCamera;
	m_menuUI.RenderAll(cmd, camera);
}

bool CMenuScene::OnProcessingMouseMessage(HWND /*hWnd*/, UINT nMessageID, WPARAM /*wParam*/, LPARAM lParam)
{
	if ( nMessageID != WM_LBUTTONDOWN ) return false;

	POINT ptClient{};
	ptClient.x = GET_X_LPARAM(lParam);
	ptClient.y = GET_Y_LPARAM(lParam);

	if ( !m_menuUI.IsPointInSprite(m_startButtonSpriteIndex, ptClient) ) return false;

	if ( m_pAudioManager ) m_pAudioManager->PlaySound2D("Assets/Audio/StartEffect.mp3", false, false, 1.0f, false);

	m_waitSceneRequested = true;
	return true;
}

bool CMenuScene::ConsumeSceneRequest(ESceneRequest& outReq)
{
	if ( !m_waitSceneRequested )
	{
		outReq = ESceneRequest::None;
		return false;
	}

	m_waitSceneRequested = false;
	outReq = ESceneRequest::SwitchToWait;
	return true;
}

void CMenuScene::OnResize(int width, int height)
{
	m_menuUI.OnResize(width, height);

	m_viewportWidth = width;
	m_viewportHeight = height;

	m_menuUI.SetSpriteRect(m_menuBackgroundSpriteIndex, XMFLOAT4(m_viewportWidth * 0.5f, m_viewportHeight * 0.5f, static_cast< float >( m_viewportWidth ), static_cast< float >( m_viewportHeight )));

	if ( const auto* start = m_menuUI.GetSprite(m_startButtonSpriteIndex) ) m_menuUI.SetSpriteRect(m_startButtonSpriteIndex, CSceneUI::MakeFitRect(start->texture, m_viewportWidth * 0.5f, m_viewportHeight * 0.78f, m_viewportWidth * 0.40f, m_viewportHeight * 0.18f));
}