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

	m_menuBackgroundSpriteIndex = m_menuUI.AddSprite(dev, cmd,
		"MenuBackground",
		L"Assets/UI/MenuImage.dds",
		CSceneUI::GetFullscreenRect(),
		CSceneUI::ELayer::Background,
		true
	);

	m_startButtonSpriteIndex = m_menuUI.AddFitSprite(dev, cmd,
		"StartButton",
		L"Assets/UI/StartButton.dds",
		FRAME_BUFFER_WIDTH * 0.5f,
		FRAME_BUFFER_HEIGHT * 0.78f,
		FRAME_BUFFER_WIDTH * 0.40f,
		FRAME_BUFFER_HEIGHT * 0.18f,
		CSceneUI::ELayer::Content,
		true
	);

	m_loadingSpriteIndex = m_menuUI.AddSprite(dev, cmd,
		"Loading",
		L"Assets/UI/LoadingImage.dds",
		CSceneUI::GetFullscreenRect(),
		CSceneUI::ELayer::Content,
		false
	);

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
	m_loadingSpriteIndex = -1;

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

	cam->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
	cam->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
	cam->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);

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
	if ( !cmd )
		return;

	if ( !camera )
		camera = m_pMainCamera;

	m_menuUI.SetSpriteVisible(m_startButtonSpriteIndex, !m_showLoading);
	m_menuUI.SetSpriteVisible(m_loadingSpriteIndex, m_showLoading);

	m_menuUI.RenderAll(cmd, camera);
}

bool CMenuScene::OnProcessingMouseMessage(HWND /*hWnd*/, UINT nMessageID, WPARAM /*wParam*/, LPARAM lParam)
{
	if ( nMessageID != WM_LBUTTONDOWN )
		return false;

	// 로딩 표시 시작 후에는 추가 입력 무시
	if ( m_showLoading )
		return true;

	POINT ptClient{};
	ptClient.x = GET_X_LPARAM(lParam);
	ptClient.y = GET_Y_LPARAM(lParam);

	// 스타트 버튼 영역 안을 눌렀을 때만 시작
	if ( !m_menuUI.IsPointInSprite(m_startButtonSpriteIndex, ptClient) )
		return false;

	if ( m_pAudioManager )
	{
		m_pAudioManager->PlaySound2D(
			"Assets/Audio/StartEffect.mp3",
			false,   // loop
			false,   // stream
			1.0f,    // volume
			false    // startPaused
		);
	}

	m_showLoading = true;
	m_startGameRequested = true;
	return true;
}

bool CMenuScene::ConsumeSceneRequest(ESceneRequest& outReq)
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