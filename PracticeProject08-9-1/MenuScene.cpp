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

	{
		// 배경 메뉴 이미지
		constexpr const wchar_t* kMenuDDS = L"Assets/UI/MenuImage.dds";

		m_menuTex = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 0);
		m_menuTex->LoadTextureFromFile(dev, cmd, kMenuDDS, RESOURCE_TEXTURE2D, 0);

		CScene::m_pDescriptorHeap->CreateShaderResourceViewsOther(
			dev,
			m_menuTex.get(),
			ROOT_PARAMETER_GLOBAL_SRV);

		m_menuSrvIndex = m_menuTex->GetSrvIndex(0);
	}

	{
		// 스타트 버튼 이미지
		constexpr const wchar_t* kStartButtonDDS = L"Assets/UI/StartButton.dds";

		m_startButtonTex = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 0);
		m_startButtonTex->LoadTextureFromFile(dev, cmd, kStartButtonDDS, RESOURCE_TEXTURE2D, 0);

		CScene::m_pDescriptorHeap->CreateShaderResourceViewsOther(
			dev,
			m_startButtonTex.get(),
			ROOT_PARAMETER_GLOBAL_SRV);

		m_startButtonSrvIndex = m_startButtonTex->GetSrvIndex(0);
	}

	{
		// 로딩 이미지
		constexpr const wchar_t* kLoadingDDS = L"Assets/UI/LoadingImage.dds";

		m_loadingTex = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 0);
		m_loadingTex->LoadTextureFromFile(dev, cmd, kLoadingDDS, RESOURCE_TEXTURE2D, 0);

		CScene::m_pDescriptorHeap->CreateShaderResourceViewsOther(
			dev,
			m_loadingTex.get(),
			ROOT_PARAMETER_GLOBAL_SRV);

		m_loadingSrvIndex = m_loadingTex->GetSrvIndex(0);
	}

	{
		m_menuShader = std::make_shared<CRectUIShader>();

		DXGI_FORMAT rtv = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT dsv = DXGI_FORMAT_UNKNOWN;

		m_menuShader->CreateShader(
			dev,
			GetGraphicsRootSignature(),
			1,
			&rtv,
			dsv);
		m_menuShader->CreateShaderVariables(dev, cmd);
	}

	if ( m_pAudioManager )
	{
		if ( auto* music = m_pAudioManager->GetMusicDirector() )
		{
			music->RequestState(EMusicState::Menu, true);
			music->BeginPendingTransition();
		}
	}
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

XMFLOAT4 CMenuScene::GetFitRect(const std::shared_ptr<CTexture>& tex, float centerX, float centerY, float maxW, float maxH) const
{
	float w = tex ? static_cast< float >( tex->GetTextureWidth(0) ) : 0.0f;
	float h = tex ? static_cast< float >( tex->GetTextureHeight(0) ) : 0.0f;

	if ( w <= 0.0f || h <= 0.0f )
	{
		w = 256.0f;
		h = 128.0f;
	}

	float scale = 1.0f;
	if ( w > maxW ) scale = min(scale, maxW / w);
	if ( h > maxH ) scale = min(scale, maxH / h);

	return XMFLOAT4(centerX, centerY, w * scale, h * scale);
}

XMFLOAT4 CMenuScene::GetFullscreenRect() const
{
	return XMFLOAT4(
		FRAME_BUFFER_WIDTH * 0.5f,
		FRAME_BUFFER_HEIGHT * 0.5f,
		static_cast< float >( FRAME_BUFFER_WIDTH ),
		static_cast< float >( FRAME_BUFFER_HEIGHT )
	);
}

XMFLOAT4 CMenuScene::GetStartButtonRect() const
{
	// 위치/크기는 여기서 조정
	return GetFitRect(
		m_startButtonTex,
		FRAME_BUFFER_WIDTH * 0.5f,
		FRAME_BUFFER_HEIGHT * 0.78f,
		FRAME_BUFFER_WIDTH * 0.40f,
		FRAME_BUFFER_HEIGHT * 0.18f);
}

XMFLOAT4 CMenuScene::GetLoadingRect() const
{
	return GetFullscreenRect();
}

bool CMenuScene::IsPointInRect(POINT pt, const XMFLOAT4& rect) const
{
	const float left = rect.x - rect.z * 0.5f;
	const float right = rect.x + rect.z * 0.5f;
	const float top = rect.y - rect.w * 0.5f;
	const float bottom = rect.y + rect.w * 0.5f;

	return ( pt.x >= left && pt.x <= right && pt.y >= top && pt.y <= bottom );
}

void CMenuScene::DrawUiTexture(ID3D12GraphicsCommandList* cmd, CCamera* camera, UINT srvIndex, const XMFLOAT4& rect)
{
	if ( !cmd || !m_menuShader || srvIndex == UINT_MAX )
		return;

	PS_CB_DRAW_OPTIONS opt{};
	opt.m_xmn4DrawOptions = XMINT4('T', 0, 0, 0);
	opt.m_xmu4PostSrvIdx0 = XMUINT4(srvIndex, 0, 0, 0);
	opt.m_xmu4PostSrvIdx1 = XMUINT4(0, 0, 0, 0);
	opt.m_xmf4UiRect = rect;
	opt.m_xmf4Viewport = XMFLOAT4(
		static_cast< float >( FRAME_BUFFER_WIDTH ),
		static_cast< float >( FRAME_BUFFER_HEIGHT ),
		1.0f / static_cast< float >( FRAME_BUFFER_WIDTH ),
		1.0f / static_cast< float >( FRAME_BUFFER_HEIGHT )
	);

	m_menuShader->Render(cmd, camera, &opt);
}

void CMenuScene::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) camera = m_pMainCamera;

	CScene::OnPrepareRender(cmd, camera);

	if ( !m_menuShader ) return;
	m_menuShader->ResetDrawOptionWriteIndex();

	// 1) 배경 메뉴 이미지
	DrawUiTexture(
		cmd,
		camera,
		m_menuSrvIndex,
		GetFullscreenRect());

	// 2) 버튼 또는 로딩 이미지
	if ( m_showLoading )
	{
		DrawUiTexture(cmd, camera, m_loadingSrvIndex, GetLoadingRect());
	}
	else
	{
		DrawUiTexture(cmd, camera, m_startButtonSrvIndex, GetStartButtonRect());
	}
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
	if ( !IsPointInRect(ptClient, GetStartButtonRect()) )
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