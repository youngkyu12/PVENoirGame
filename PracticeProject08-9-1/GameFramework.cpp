//-----------------------------------------------------------------------------
// File: CGameFramework.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameFramework.h"

#include "Scene.h"
#include "GameScene.h"
#include "AssetManager.h"
#include "AnimController.h"
#include "AnimatorComponent.h"
#include "PlayerControllerComponent.h"
#include "AudioManager.h"
#include "MusicDirector.h"
#include "Camera.h"
#include "Object.h"

#include "Service.h"
#include "ServerPacketHandler.h"
#include "GlobalValues.h"

#include <chrono>

CGameFramework::CGameFramework()
{
	_tcscpy_s(m_pszFrameRate, _T("LabProject ("));
}

CGameFramework::~CGameFramework()
{
}

bool CGameFramework::OnCreate(HINSTANCE hInstance, HWND hMainWnd)
{
	m_hInstance = hInstance;
	m_hWnd = hMainWnd;
	m_bWindowActive = IsWindowActuallyActive();
	m_bUserPaused = false;
	
	CreateDirect3DDevice();
	CreateCommandQueueAndList();
	CreateRtvAndDsvDescriptorHeaps();

	CreateSwapChain();
#ifndef _WITH_SWAPCHAIN_FULLSCREEN_STATE
	CreateSwapChainRenderTargetViews();
#endif
	CreateDepthStencilView();

	constexpr UINT GLOBAL_CBV_CAPACITY = 8192;

	CScene::m_pDescriptorHeap->CreateCbvSrvDescriptorHeaps(
		m_pd3dDevice.Get(),
		GLOBAL_CBV_CAPACITY,
		GLOBAL_SRV_CAPACITY);

	m_pAudioManager = std::make_unique<CAudioManager>();
	m_pAudioManager->Initialize();

	if ( auto* music = m_pAudioManager->GetMusicDirector() )
	{
		music->RegisterMusic(EMusicState::Menu, "Assets/Audio/MainMenuBGM_Test.mp3");
		music->RegisterMusic(EMusicState::Gameplay, "Assets/Audio/ForestBGMWithBird.wav");
		music->SetCrossFadeSeconds(1.5f);
	}

	m_SceneManager.SetAudioManager(m_pAudioManager.get());

	BuildObjects();

	return( true );
}

void CGameFramework::OnDestroy()
{
	ReleaseObjects();

	if ( m_pAudioManager )
	{
		m_pAudioManager->Shutdown();
		m_pAudioManager.reset();
	}

	AssetManager::ClearCache();

	::CloseHandle(m_hFenceEvent);

	if (m_pd3dDepthStencilBuffer)
		m_pd3dDepthStencilBuffer.Reset();

	if (m_pd3dDsvDescriptorHeap)
		m_pd3dDsvDescriptorHeap.Reset();

	for (int i = 0; i < m_nSwapChainBuffers; i++)
		if (m_ppd3dSwapChainBackBuffers[i])
			m_ppd3dSwapChainBackBuffers[i].Reset();

	if (m_pd3dRtvDescriptorHeap)
		m_pd3dRtvDescriptorHeap.Reset();

	if (m_pd3dCommandAllocator)
		m_pd3dCommandAllocator.Reset();

	if (m_pd3dCommandQueue)
		m_pd3dCommandQueue.Reset();

	if (m_pd3dCommandList)
		m_pd3dCommandList.Reset();

	if (m_pd3dFence)
		m_pd3dFence.Reset();

	if (m_pdxgiSwapChain)
		m_pdxgiSwapChain->SetFullscreenState(FALSE, nullptr);

	if (m_pdxgiSwapChain)
		m_pdxgiSwapChain.Reset();

	if (m_pd3dDevice)
		m_pd3dDevice.Reset();

	if (m_pdxgiFactory)
		m_pdxgiFactory.Reset();

#if defined(_DEBUG)
	ComPtr<IDXGIDebug1>	pdxgiDebug;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pdxgiDebug));
	HRESULT hResult = pdxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
	(void)hResult;
	pdxgiDebug.Reset();
#endif
}

void CGameFramework::ReleaseObjects()
{
	WaitForGpuComplete();

	m_SceneManager.ReleaseCurrent();
	m_pCamera = nullptr;

	if (m_pPostProcessingShader)
	{
		m_pPostProcessingShader->ReleaseObjects();
		m_pPostProcessingShader.reset();
	}
}

void CGameFramework::WaitForGpuComplete()
{
	PROFILE_RENDER_SCOPE("Framework::WaitForGpuComplete(total)");

	const UINT64 nFenceValue = ++m_nFenceValues[m_nSwapChainBufferIndex];
	HRESULT hResult = m_pd3dCommandQueue->Signal(m_pd3dFence.Get(), nFenceValue);

	if ( m_pd3dFence->GetCompletedValue() < nFenceValue )
	{
		hResult = m_pd3dFence->SetEventOnCompletion(nFenceValue, m_hFenceEvent);
		::WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}

void CGameFramework::CreateDirect3DDevice()
{
	HRESULT hResult;

	UINT nDXGIFactoryFlags = 0;
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> pd3dDebugController;
	hResult = D3D12GetDebugInterface(IID_PPV_ARGS(&pd3dDebugController));
	if (pd3dDebugController)
	{
		pd3dDebugController->EnableDebugLayer();
		pd3dDebugController.Reset();
	}
	nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	hResult = ::CreateDXGIFactory2(
		nDXGIFactoryFlags,
		IID_PPV_ARGS(&m_pdxgiFactory));

	ComPtr<IDXGIAdapter1> pd3dAdapter;
	if ( FAILED(hResult) )
		return;

	for ( UINT i = 0; ; ++i )
	{
		
		hResult = m_pdxgiFactory->EnumAdapterByGpuPreference(
			i,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&pd3dAdapter));

		if ( hResult == DXGI_ERROR_NOT_FOUND )
			break;

		if ( FAILED(hResult) )
			continue;

		DXGI_ADAPTER_DESC1 desc = {};
		pd3dAdapter->GetDesc1(&desc);

		if ( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE )
			continue;

		hResult = D3D12CreateDevice(
			pd3dAdapter.Get(),
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&m_pd3dDevice));

		if ( SUCCEEDED(hResult) )
			break;
	}
	
	if (!m_pd3dDevice)
	{
		hResult = m_pdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pd3dAdapter));
		hResult = D3D12CreateDevice(
			pd3dAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_pd3dDevice));
	}

	if (!m_pd3dDevice)
	{
		MessageBox(nullptr, L"Direct3D 12 Device Cannot be Created.", L"Error", MB_OK);
		::PostQuitMessage(0);
		return;
	}

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS d3dMsaaQualityLevels;
	d3dMsaaQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3dMsaaQualityLevels.SampleCount = 4;
	d3dMsaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	d3dMsaaQualityLevels.NumQualityLevels = 0;
	hResult = m_pd3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&d3dMsaaQualityLevels,
		sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS));

	m_nMsaa4xQualityLevels = d3dMsaaQualityLevels.NumQualityLevels;
	m_bMsaa4xEnable = (m_nMsaa4xQualityLevels > 1) ? true : false;

	hResult = m_pd3dDevice->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_pd3dFence));

	for (UINT i = 0; i < m_nSwapChainBuffers; i++)
		m_nFenceValues[i] = 1;

	m_hFenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

	::gnRtvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	::gnCbvSrvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void CGameFramework::CreateCommandQueueAndList()
{
	HRESULT hResult;
#if defined(_DEBUG)
	m_pd3dDevice->QueryInterface(IID_PPV_ARGS(&infoQueue));

	// 치명적인 문제(CORRUPTION) 디버깅 종료
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);

	// 에러(ERROR) 디버깅 종료
	//infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

	// 경고(WARNING) 무시
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);
#endif
	

	D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
	::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hResult = m_pd3dDevice->CreateCommandQueue(
		&d3dCommandQueueDesc,
		IID_PPV_ARGS(&m_pd3dCommandQueue));

	hResult = m_pd3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&m_pd3dCommandAllocator));

	hResult = m_pd3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_pd3dCommandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&m_pd3dCommandList));

	hResult = m_pd3dCommandList->Close();
}

void CGameFramework::CreateRtvAndDsvDescriptorHeaps()
{
	HRESULT hResult;

	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
	::ZeroMemory(&d3dDescriptorHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	d3dDescriptorHeapDesc.NumDescriptors = m_nSwapChainBuffers + 5;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	d3dDescriptorHeapDesc.NodeMask = 0;

	hResult = m_pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(&m_pd3dRtvDescriptorHeap));

	d3dDescriptorHeapDesc.NumDescriptors = 1;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

	hResult = m_pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(&m_pd3dDsvDescriptorHeap));
}

void CGameFramework::CreateSwapChain()
{
	HRESULT hResult;

	DXGI_SWAP_CHAIN_DESC dxgiSwapChainDesc;
	::ZeroMemory(&dxgiSwapChainDesc, sizeof(dxgiSwapChainDesc));
	dxgiSwapChainDesc.BufferCount = m_nSwapChainBuffers;
	dxgiSwapChainDesc.BufferDesc.Width = m_nWndClientWidth;
	dxgiSwapChainDesc.BufferDesc.Height = m_nWndClientHeight;
	dxgiSwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	dxgiSwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiSwapChainDesc.OutputWindow = m_hWnd;
	dxgiSwapChainDesc.SampleDesc.Count = (m_bMsaa4xEnable) ? 4 : 1;
	dxgiSwapChainDesc.SampleDesc.Quality = (m_bMsaa4xEnable) ? (m_nMsaa4xQualityLevels - 1) : 0;
	dxgiSwapChainDesc.Windowed = TRUE;
#ifdef _WITH_ONLY_RESIZE_BACKBUFFERS
	dxgiSwapChainDesc.Flags = 0;
#else
	dxgiSwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
#endif

	hResult = m_pdxgiFactory->CreateSwapChain(
		m_pd3dCommandQueue.Get(),
		&dxgiSwapChainDesc,
		(IDXGISwapChain**)m_pdxgiSwapChain.ReleaseAndGetAddressOf());

	hResult = m_pdxgiFactory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);
	m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();
}

void CGameFramework::CreateSwapChainRenderTargetViews()
{
	D3D12_RENDER_TARGET_VIEW_DESC d3dRenderTargetViewDesc;
	d3dRenderTargetViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3dRenderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	d3dRenderTargetViewDesc.Texture2D.MipSlice = 0;
	d3dRenderTargetViewDesc.Texture2D.PlaneSlice = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < m_nSwapChainBuffers; i++)
	{
		m_pdxgiSwapChain->GetBuffer(
			i,
			IID_PPV_ARGS(m_ppd3dSwapChainBackBuffers[i].ReleaseAndGetAddressOf()));

		m_pd3dDevice->CreateRenderTargetView(
			m_ppd3dSwapChainBackBuffers[i].Get(),
			&d3dRenderTargetViewDesc,
			d3dRtvCPUDescriptorHandle);

		m_pd3dSwapChainBackBufferRTVCPUHandles[i] = d3dRtvCPUDescriptorHandle;
		d3dRtvCPUDescriptorHandle.ptr += ::gnRtvDescriptorIncrementSize;
	}
}

void CGameFramework::CreateDepthStencilView()
{
	D3D12_RESOURCE_DESC d3dResourceDesc;
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3dResourceDesc.Alignment = 0;
	d3dResourceDesc.Width = m_nWndClientWidth;
	d3dResourceDesc.Height = m_nWndClientHeight;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dResourceDesc.SampleDesc.Count = (m_bMsaa4xEnable) ? 4 : 1;
	d3dResourceDesc.SampleDesc.Quality = (m_bMsaa4xEnable) ? (m_nMsaa4xQualityLevels - 1) : 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES d3dHeapProperties;
	::ZeroMemory(&d3dHeapProperties, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	d3dHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapProperties.CreationNodeMask = 1;
	d3dHeapProperties.VisibleNodeMask = 1;

	D3D12_CLEAR_VALUE d3dClearValue;
	d3dClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dClearValue.DepthStencil.Depth = 1.0f;
	d3dClearValue.DepthStencil.Stencil = 0;

	m_pd3dDevice->CreateCommittedResource(
		&d3dHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&d3dClearValue,
		IID_PPV_ARGS(m_pd3dDepthStencilBuffer.ReleaseAndGetAddressOf())
	);

	m_d3dDsvDescriptorCPUHandle = m_pd3dDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	m_pd3dDevice->CreateDepthStencilView(
		m_pd3dDepthStencilBuffer.Get(),
		nullptr,
		m_d3dDsvDescriptorCPUHandle
	);
}

void CGameFramework::BuildObjects()
{
	BuildSceneInternal(ESceneId::Menu, true);
}

void CGameFramework::SyncGameSceneInactiveOverlay()
{
	CScene* scene = m_SceneManager.GetScene();
	if (!scene) 
		return;

	CGameScene* gameScene = dynamic_cast<CGameScene*>(scene);
	if (!gameScene) 
		return;

	gameScene->SetInactiveOverlayVisible(IsInputPauseActive());
}

void CGameFramework::ClearInputPause()
{
	m_bUserPaused = false;
	m_bConsumeNextMouseClick = false;

	::GetCursorPos(&m_ptOldCursorPos);
	SyncGameSceneInactiveOverlay();
}

bool CGameFramework::HandlePauseClick(UINT nMessageID, LPARAM lParam)
{
	if ((nMessageID != WM_LBUTTONDOWN) && (nMessageID != WM_RBUTTONDOWN))
		return false;

	if (!IsInputPauseActive())
		return false;

	CGameScene* gameScene = dynamic_cast<CGameScene*>(m_SceneManager.GetScene());

	POINT ptClient = {};
	ptClient.x = GET_X_LPARAM(lParam);
	ptClient.y = GET_Y_LPARAM(lParam);

	// GameScene이고 Pause UI 위를 클릭했으면 종료
	if (gameScene && gameScene->IsPointInPauseOverlay(ptClient))
	{
		g_End.store(true);
		::PostQuitMessage(0);
		return true;
	}

	// Pause UI 바깥 클릭이면 입력정지 해제
	ClearInputPause();
	return true;
}

bool CGameFramework::IsWindowActuallyActive() const
{
	return (m_hWnd != nullptr) && (::GetForegroundWindow() == m_hWnd);
}

bool CGameFramework::IsInputPauseActive() const
{
	return m_bUserPaused;
}

void CGameFramework::UpdateAudioListener()
{
	if ( !m_pAudioManager || !m_pAudioManager->IsInitialized() )
		return;

	CScene* scene = m_SceneManager.GetScene();

	CGameObject* localPlayer = scene ? scene->GetPlayer() : nullptr;

	XMFLOAT3 listenerPos(0.0f, 0.0f, 0.0f);

	if ( localPlayer )
	{
		// 플레이어 머리/귀 높이 근처.
		listenerPos = localPlayer->GetPosition();
		listenerPos.y += 1.7f;
	}
	else if ( m_pCamera )
	{
		listenerPos = m_pCamera->GetPosition();
	}
	else
	{
		return;
	}

	XMFLOAT3 forward(0.0f, 0.0f, 1.0f);
	XMFLOAT3 up(0.0f, 1.0f, 0.0f);

	if ( m_pCamera )
	{
		forward = m_pCamera->GetLookVector();
		up = m_pCamera->GetUpVector();
	}

	const XMFLOAT3 velocity(0.0f, 0.0f, 0.0f);

	m_pAudioManager->SetListenerAttributes(
		listenerPos,
		velocity,
		forward,
		up
	);
}

void CGameFramework::UpdateWindowActivationState()
{
	const bool isActiveNow = IsWindowActuallyActive();

	if (isActiveNow == m_bWindowActive)
		return;

	m_bWindowActive = isActiveNow;

	if (!m_bWindowActive)
	{
		::ReleaseCapture();

		// 비활성화 = ESC pause와 동일하게 처리
		if (dynamic_cast<CGameScene*>(m_SceneManager.GetScene()))
			m_bUserPaused = true;

		SyncGameSceneInactiveOverlay();
	}
	else
	{
		::GetCursorPos(&m_ptOldCursorPos);

		// 다시 클릭해서 활성화될 때 첫 클릭은
		// 오직 활성화만 하고 어떤 pause 처리도 하지 않음
		m_bConsumeNextMouseClick = true;

		SyncGameSceneInactiveOverlay();
	}
}

void CGameFramework::BuildSceneInternal(ESceneId id, bool resetTimer)
{

	WaitForGpuComplete();
	
	m_SceneManager.ReleaseCurrent();
	m_pCamera = nullptr;

	if (m_pPostProcessingShader)
	{
		m_pPostProcessingShader->ReleaseObjects();
		m_pPostProcessingShader.reset();
	}

	HRESULT hr = m_pd3dCommandAllocator->Reset();
	(void)hr;
	hr = m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), nullptr);
	(void)hr;

	m_SceneManager.BuildScene(id, m_pd3dDevice.Get(), m_pd3dCommandList.Get());

	CScene* scene = m_SceneManager.GetScene();
	if (!scene)
	{
		m_pd3dCommandList->Close();
		return;
	}

	m_bUserPaused = false;
	m_bConsumeNextMouseClick = false;

	m_pCamera = scene->GetMainCamera();
	SyncGameSceneInactiveOverlay();

	m_pPostProcessingShader = make_shared<CTextureToFullScreenShader>(); 
	m_pPostProcessingShader->CreateShader(
		m_pd3dDevice.Get(),
		scene->GetGraphicsRootSignature(),
		1,
		nullptr,
		DXGI_FORMAT_D24_UNORM_S8_UINT);
	m_pPostProcessingShader->BuildObjects(
		m_pd3dDevice.Get(), 
		m_pd3dCommandList.Get(),
		&m_nDrawOption);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (::gnRtvDescriptorIncrementSize * m_nSwapChainBuffers);

	DXGI_FORMAT pdxgiResourceFormats[5] = {
	DXGI_FORMAT_R8G8B8A8_UNORM, // SV_TARGET0 : color
	DXGI_FORMAT_R8G8B8A8_UNORM, // SV_TARGET1 : cTexture
	DXGI_FORMAT_R8G8B8A8_UNORM, // SV_TARGET2 : cIllumination
	DXGI_FORMAT_R8G8B8A8_UNORM, // SV_TARGET3 : normal
	DXGI_FORMAT_R32_FLOAT       // SV_TARGET4 : zDepth
	};

	m_pPostProcessingShader->CreateResourcesAndRtvsSrvs(
		m_pd3dDevice.Get(),
		m_pd3dCommandList.Get(),
		5,
		pdxgiResourceFormats,
		d3dRtvCPUDescriptorHandle);

	D3D12_GPU_DESCRIPTOR_HANDLE d3dDsvGPUDescriptorHandle = CScene::m_pDescriptorHeap->CreateShaderResourceView(
		m_pd3dDevice.Get(),
		m_pd3dDepthStencilBuffer.Get(),
		DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
	(void)d3dDsvGPUDescriptorHandle;

	if ( CGameScene* gameScene = dynamic_cast< CGameScene* >( scene ) )
	{
		gameScene->SetDepthFogSourceSrvIndices(
			m_pPostProcessingShader->GetTexture()->GetSrvIndex(0),
			m_pPostProcessingShader->GetTexture()->GetSrvIndex(4)
		);
	}

	hr = m_pd3dCommandList->Close();
	(void)hr;

	ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
	m_pd3dCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
	WaitForGpuComplete();

	if (scene)
		scene->ReleaseUploadBuffers();

	if (resetTimer)
		m_GameTimer.Reset();
	else
		m_GameTimer.Reset();


	
}

void CGameFramework::RequestSceneSwitch(ESceneId next, bool presentCurrentSceneOnceBeforeSwitch)
{
	m_sceneSwitchPending = true;
	m_pendingScene = next;

	m_sceneSwitchReadyToApply = !presentCurrentSceneOnceBeforeSwitch;
}

void CGameFramework::ApplyPendingSceneSwitch()
{
	if ( !m_sceneSwitchPending )
		return;

	if ( !m_sceneSwitchReadyToApply )
		return;

	const ESceneId next = m_pendingScene;

	m_sceneSwitchPending = false;
	m_sceneSwitchReadyToApply = false;
	m_pendingScene = ESceneId::Menu;

	BuildSceneInternal(next, true);
}

void CGameFramework::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	UpdateWindowActivationState();
	if (!m_bWindowActive)
		return;

	const bool isButtonMsg =
		(nMessageID == WM_LBUTTONDOWN) ||
		(nMessageID == WM_RBUTTONDOWN) ||
		(nMessageID == WM_LBUTTONUP) ||
		(nMessageID == WM_RBUTTONUP);

	// 비활성 -> 활성 직후 첫 클릭은 무조건 버린다.
	// (활성화만 하고, pause 해제/종료 판정도 하지 않음)
	if (m_bConsumeNextMouseClick && isButtonMsg)
	{
		m_bConsumeNextMouseClick = false;
		::GetCursorPos(&m_ptOldCursorPos);
		return;
	}

	// pause 상태면 scene 입력보다 pause 클릭 처리가 먼저다.
	if (IsInputPauseActive())
	{
		if (HandlePauseClick(nMessageID, lParam))
			return;

		return;
	}

	CScene* scene = m_SceneManager.GetScene();
	if (scene) scene->OnProcessingMouseMessage(hWnd, nMessageID, wParam, lParam);

	if ( scene )
	{
		CScene::ESceneRequest req;
		if ( scene->ConsumeSceneRequest(req) )
		{
			if ( req == CScene::ESceneRequest::SwitchToGame )
			{
				// MenuScene을 "로딩 UI 상태"로 1프레임 더 보여준 뒤 실제 전환
				RequestSceneSwitch(ESceneId::Game, true);
			}
		}
	}

	switch (nMessageID)
	{
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
		::SetCapture(hWnd);
		::GetCursorPos(&m_ptOldCursorPos);
		break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		::ReleaseCapture();
		break;
	case WM_MOUSEMOVE:
		break;
	default:
		break;
	}
}

void CGameFramework::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	UpdateWindowActivationState();
	if (!m_bWindowActive)
		return;

	if (nMessageID == WM_KEYUP)
	{
		if (wParam == VK_ESCAPE)
		{
			if (dynamic_cast<CGameScene*>(m_SceneManager.GetScene()))
			{
				m_bUserPaused = !m_bUserPaused;
				SyncGameSceneInactiveOverlay();
			}

			// ::PostQuitMessage(0);
			return;
		}
	}

	if (IsInputPauseActive())
		return;

	CScene* scene = m_SceneManager.GetScene();
	if (scene) scene->OnProcessingKeyboardMessage(hWnd, nMessageID, wParam, lParam);

	switch (nMessageID)
	{
	case WM_KEYUP:
		switch (wParam)
		{
		case VK_F9:
			ChangeSwapChainState();
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

LRESULT CALLBACK CGameFramework::OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_ACTIVATE:
	{
		UpdateWindowActivationState();
		break;
	}
	case WM_ACTIVATEAPP:
	{
		UpdateWindowActivationState();
		break;
	}
	case WM_SIZE:
		break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
		OnProcessingMouseMessage(hWnd, nMessageID, wParam, lParam);
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		OnProcessingKeyboardMessage(hWnd, nMessageID, wParam, lParam);
		break;
	}
	return(0);
}

void CGameFramework::ChangeSwapChainState()
{
	WaitForGpuComplete();

	BOOL bFullScreenState = FALSE;
	m_pdxgiSwapChain->GetFullscreenState(&bFullScreenState, NULL);
	m_pdxgiSwapChain->SetFullscreenState(!bFullScreenState, NULL);

	DXGI_MODE_DESC dxgiTargetParameters;
	dxgiTargetParameters.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiTargetParameters.Width = m_nWndClientWidth;
	dxgiTargetParameters.Height = m_nWndClientHeight;
	dxgiTargetParameters.RefreshRate.Numerator = 60;
	dxgiTargetParameters.RefreshRate.Denominator = 1;
	dxgiTargetParameters.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	dxgiTargetParameters.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	m_pdxgiSwapChain->ResizeTarget(&dxgiTargetParameters);

	for (int i = 0; i < m_nSwapChainBuffers; i++)
		if (m_ppd3dSwapChainBackBuffers[i])
			m_ppd3dSwapChainBackBuffers[i].Reset();

	DXGI_SWAP_CHAIN_DESC dxgiSwapChainDesc;
	m_pdxgiSwapChain->GetDesc(&dxgiSwapChainDesc);
	m_pdxgiSwapChain->ResizeBuffers(
		m_nSwapChainBuffers,
		m_nWndClientWidth,
		m_nWndClientHeight,
		dxgiSwapChainDesc.BufferDesc.Format,
		dxgiSwapChainDesc.Flags
	);

	m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();

	CreateSwapChainRenderTargetViews();
}

void CGameFramework::ProcessInput()
{
	UpdateWindowActivationState();
	if ( IsInputPauseActive() )
		return;

	static UCHAR pKeysBuffer[256];
	bool bProcessedByScene = false;

	CScene* scene = m_SceneManager.GetScene();
	if ( GetKeyboardState(pKeysBuffer) && scene )
		bProcessedByScene = scene->ProcessInput(pKeysBuffer);

	// Demo: 0/1/2/3 -> Player slot(0/1/2/3) Attack (edge trigger)
	static bool s_prevDown[4] = { false, false, false, false };
	for ( int slot = 0; slot < 4; ++slot )
	{
		// 0 키는 총 사운드 튜닝용으로 사용.
		if ( slot == 0 )
			continue;

		const bool down = ( pKeysBuffer['0' + slot] & 0xF0 ) != 0;
		if ( down && !s_prevDown[slot] )
		{
			if ( scene ) scene->RequestPlayerAttackBySlot(slot);
		}
		s_prevDown[slot] = down;
	}

	CGameObject* playerObj = ( scene ? scene->GetPlayer() : nullptr );
	if ( !playerObj ) return;

	auto* pc = playerObj->GetComponent<CPlayerControllerComponent>();
	if ( !pc ) return;

	DWORD dwDirection = 0;
	float cxDelta = 0.0f;
	float cyDelta = 0.0f;
	bool bRunRequested = false;

	if ( !bProcessedByScene )
	{
		Protocol::C_INPUT inputPkt;

#ifdef USING_NETWORK
		// Pack keyBuffer into int32 (WASD 기준)
		int keyCodes = 0;
		if (pKeysBuffer['W'] & 0xF0)      keyCodes |= (1 << 0); // Forward
		if (pKeysBuffer[VK_UP] & 0xF0)    keyCodes |= (1 << 0);
		if (pKeysBuffer['S'] & 0xF0)      keyCodes |= (1 << 1); // Backward
		if (pKeysBuffer[VK_DOWN] & 0xF0)  keyCodes |= (1 << 1);
		if (pKeysBuffer['A'] & 0xF0)      keyCodes |= (1 << 2); // Left
		if (pKeysBuffer[VK_LEFT] & 0xF0)  keyCodes |= (1 << 2);
		if (pKeysBuffer['D'] & 0xF0)      keyCodes |= (1 << 3); // Right
		if (pKeysBuffer[VK_RIGHT] & 0xF0) keyCodes |= (1 << 3);
		if (pKeysBuffer[VK_PRIOR] & 0xF0) keyCodes |= (1 << 4); // Up
		if (pKeysBuffer[VK_NEXT] & 0xF0)  keyCodes |= (1 << 5); // Down

		if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) keyCodes |= (1 << 6);
		if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) keyCodes |= (1 << 7);

		if (pKeysBuffer[VK_LSHIFT] & 0xF0 || pKeysBuffer[VK_SHIFT] & 0xF0)    keyCodes |= (1 << 8); // Run
		if (pKeysBuffer[VK_SPACE] & 0xF0) keyCodes |= (1 << 9); // Roll

		inputPkt.set_playerid(g_myPlayerId);
		inputPkt.set_keycodes(keyCodes);

#endif
		// 클라이언트 로컬 애니메이션 반영 코드를 살림

		if (pKeysBuffer['W'] & 0xF0) dwDirection |= DIR_FORWARD;

		if (pKeysBuffer['S'] & 0xF0) dwDirection |= DIR_BACKWARD;
		if (pKeysBuffer['A'] & 0xF0) dwDirection |= DIR_LEFT;
		if (pKeysBuffer['D'] & 0xF0) dwDirection |= DIR_RIGHT;
		if (pKeysBuffer[VK_PRIOR] & 0xF0) dwDirection |= DIR_UP;
		if (pKeysBuffer[VK_NEXT] & 0xF0)  dwDirection |= DIR_DOWN;

		bRunRequested =
			( ( pKeysBuffer[VK_LSHIFT] & 0xF0 ) != 0 ) ||
			( ( pKeysBuffer[VK_SHIFT] & 0xF0 ) != 0 );

		static bool s_prevSpaceDown = false;
		const bool spaceDown = ( pKeysBuffer[VK_SPACE] & 0xF0 ) != 0;

		if ( spaceDown && !s_prevSpaceDown )
		{
			bool requestedRoll = false;

			if ( auto* animComp = playerObj->GetComponent<CAnimatorComponent>() )
			{
				if ( auto* ctrl = animComp->EnsureController() )
				{
					ctrl->RequestRoll(static_cast< uint32_t >( dwDirection ));
					requestedRoll = true;
				}
			}
			else if ( auto* ctrl = playerObj->GetAnimController() )
			{
				ctrl->RequestRoll(static_cast< uint32_t >( dwDirection ));
				requestedRoll = true;
			}

			if ( requestedRoll )
			{
				if ( auto* equip = playerObj->GetComponent<CPlayerEquipmentComponent>() )
					equip->RequestRollSfx(static_cast< uint32_t >( dwDirection ));
			}
		}

		s_prevSpaceDown = spaceDown;

		POINT ptCursorPos{};
		if ( GetCapture() == m_hWnd )
		{
			SetCursor(NULL);
			GetCursorPos(&ptCursorPos);

			cxDelta = ( float ) ( ptCursorPos.x - m_ptOldCursorPos.x ) / 3.0f;
			cyDelta = ( float ) ( ptCursorPos.y - m_ptOldCursorPos.y ) / 3.0f;

			SetCursorPos(m_ptOldCursorPos.x, m_ptOldCursorPos.y);
		}
		else
		{
			GetCursorPos(&ptCursorPos);

			cxDelta = ( float ) ( ptCursorPos.x - m_ptOldCursorPos.x ) / 3.0f;
			cyDelta = ( float ) ( ptCursorPos.y - m_ptOldCursorPos.y ) / 3.0f;

			m_ptOldCursorPos = ptCursorPos;
		}

#ifdef USING_NETWORK
		inputPkt.set_deltax(cxDelta);
		inputPkt.set_deltay(cyDelta);

		auto sendBuffer = ServerPacketHandler::MakeSendBuffer(inputPkt);
		g_clientService->BroadCast(sendBuffer);

		const float dt = m_GameTimer.GetTimeElapsed();

		// --------------------------------------------------------------------
		// 1) 카메라는 항상 마우스로 회전한다. (공격/구르기 중에도 가능)
		// --------------------------------------------------------------------
		if ( m_pCamera && ( cxDelta != 0.0f || cyDelta != 0.0f ) )
		{
			m_pCamera->Rotate(cyDelta, cxDelta, 0.0f);
		}

		const float cameraYawDeg = m_pCamera ? m_pCamera->GetYaw() : pc->GetYawDegrees();

		if ( pc->ShouldFaceCameraWhileActionActive() )
		{
			pc->SetYawDegrees(cameraYawDeg);
		}
		else if ( ( dwDirection & ( DIR_FORWARD | DIR_BACKWARD | DIR_LEFT | DIR_RIGHT ) ) &&
				 !pc->IsActionLockedByAnimation() )
		{
			pc->RotateTowardYawDegrees(cameraYawDeg, 12.0f, dt);
		}

		pc->SetRunRequested(bRunRequested);

		if ( dwDirection && !pc->IsActionLockedByAnimation() )
		{
			const XMFLOAT3 prevPos = playerObj->GetPosition();
			pc->MoveByYaw(dwDirection, 5.0f * dt, cameraYawDeg, false);
			if ( CGameScene* gameScene = dynamic_cast< CGameScene* >( scene ) )
				gameScene->RollbackLocalPlayerMoveIfCollidingWorldStatic(prevPos);
		}

		pc->SetInputDirection(static_cast< uint32_t >( dwDirection ));

		XMFLOAT3 cameraTarget = playerObj->GetPosition();
		cameraTarget.y += 1.7f;

		// --------------------------------------------------------------------
		// 2) 카메라는 항상 현재 target 위치 기준으로 다시 계산
		// --------------------------------------------------------------------
		if ( m_pCamera )
		{
			m_pCamera->Update(cameraTarget, dt);
			m_pCamera->SetLookAt(cameraTarget);
			m_pCamera->RegenerateViewMatrix();
		}
#else
		const float dt = m_GameTimer.GetTimeElapsed();

		// --------------------------------------------------------------------
		// 1) 카메라는 항상 마우스로 회전한다. (공격/구르기 중에도 가능)
		// --------------------------------------------------------------------
		if ( m_pCamera && ( cxDelta != 0.0f || cyDelta != 0.0f ) )
		{
			m_pCamera->Rotate(cyDelta, cxDelta, 0.0f);
		}

		const float cameraYawDeg = m_pCamera ? m_pCamera->GetYaw() : pc->GetYawDegrees();

		// --------------------------------------------------------------------
		// 2) 플레이어 회전 규칙
		//    - 일반 상태: 이동 중일 때만 카메라 yaw를 향해 회전
		//    - Sword/Axe 공격, Roll: 회전 금지
		//    - Bow/Gun 공격: 현행 유지 -> 공격 중에도 카메라 yaw를 따라감
		// --------------------------------------------------------------------
		if ( pc->ShouldFaceCameraWhileActionActive() )
		{
			pc->SetYawDegrees(cameraYawDeg);
		}
		else if ( ( dwDirection & ( DIR_FORWARD | DIR_BACKWARD | DIR_LEFT | DIR_RIGHT ) ) &&
				 !pc->IsActionLockedByAnimation() )
		{
			pc->RotateTowardYawDegrees(cameraYawDeg, 12.0f, dt);
		}

		// --------------------------------------------------------------------
		// 3) 이동은 카메라 yaw 기준으로 계산
		// --------------------------------------------------------------------
		pc->SetRunRequested(bRunRequested);

		if ( dwDirection && !pc->IsActionLockedByAnimation() )
		{
			const XMFLOAT3 prevPos = playerObj->GetPosition();

			const float moveSpeed = bRunRequested ? 100.0f : 5.0f;
			pc->MoveByYaw(dwDirection, moveSpeed * dt, cameraYawDeg, false);

			if ( CGameScene* gameScene = dynamic_cast< CGameScene* >( scene ) )
			{
				gameScene->RollbackLocalPlayerMoveIfCollidingWorldStatic(prevPos);
			}
		}

		// 애니메이터 방향 비트는 항상 갱신
		pc->SetInputDirection(static_cast< uint32_t >( dwDirection ));

		XMFLOAT3 cameraTarget = playerObj->GetPosition();
		cameraTarget.y += 1.7f;

		// --------------------------------------------------------------------
		// 4) 카메라는 항상 현재 target 위치 기준으로 다시 계산
		// --------------------------------------------------------------------
		if ( m_pCamera )
		{
			m_pCamera->Update(cameraTarget, dt);
			m_pCamera->SetLookAt(cameraTarget);
			m_pCamera->RegenerateViewMatrix();
		}
#endif
	}
}

void CGameFramework::AnimateObjects()
{
	PROFILE_RENDER_SCOPE("Framework::AnimateObjects");

	CScene* scene = m_SceneManager.GetScene();
	if ( scene ) scene->AnimateObjects(m_GameTimer.GetTimeElapsed());
}

void CGameFramework::CollisionSystem()
{
	PROFILE_RENDER_SCOPE("Framework::CollisionSystem");

	CScene* scene = m_SceneManager.GetScene();
	if ( scene ) scene->CollisionObjects();
}

void CGameFramework::MoveToNextFrame()
{
	m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();

	UINT64 nFenceValue = ++m_nFenceValues[m_nSwapChainBufferIndex];
	HRESULT hResult = m_pd3dCommandQueue->Signal(m_pd3dFence.Get(), nFenceValue);

	if (m_pd3dFence->GetCompletedValue() < nFenceValue)
	{
		hResult = m_pd3dFence->SetEventOnCompletion(nFenceValue, m_hFenceEvent);
		::WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}

void CGameFramework::FrameAdvance()
{
	PROFILE_RENDER_SCOPE("Framework::FrameAdvance(total)");

	HRESULT hResult;


	m_GameTimer.Tick(0.0f);
	UpdateWindowActivationState();

	ApplyPendingSceneSwitch();


#ifndef USING_NETWORK
	XMFLOAT3 localPlayerPrevPos = XMFLOAT3(0.0f, 0.0f, 0.0f);
	bool hasLocalPlayerPrevPos = false;

	if ( CGameScene* gameScene = dynamic_cast< CGameScene* >( m_SceneManager.GetScene() ) )
	{
		if ( CGameObject* localPlayer = gameScene->GetPlayer() )
		{
			localPlayerPrevPos = localPlayer->GetPosition();
			hasLocalPlayerPrevPos = true;
		}
	}
#endif

	{
		PROFILE_RENDER_SCOPE("Framework::FrameAdvance::ProcessInput");
		ProcessInput();
	}

	{
		PROFILE_RENDER_SCOPE("Framework::FrameAdvance::AnimateObjects");
		AnimateObjects();
	}

	UpdateAudioListener();

	if ( m_pAudioManager )
		m_pAudioManager->Update();

#ifndef USING_NETWORK
	if ( hasLocalPlayerPrevPos )
	{
		if ( CGameScene* gameScene = dynamic_cast< CGameScene* >( m_SceneManager.GetScene() ) )
		{
			gameScene->RollbackLocalPlayerMoveIfCollidingWorldStatic(localPlayerPrevPos);
		}
	}
#endif
	{
		PROFILE_RENDER_SCOPE("Framework::FrameAdvance::CollisionSystem");
		CollisionSystem();
	}

	{
		PROFILE_RENDER_SCOPE("Framework::FrameAdvance::CommandAllocatorAndListReset");
		hResult = m_pd3dCommandAllocator->Reset();
		hResult = m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), nullptr);
	}

	::SynchronizeResourceTransition(
		m_pd3dCommandList.Get(),
		m_ppd3dSwapChainBackBuffers[m_nSwapChainBufferIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	CScene* scene = m_SceneManager.GetScene();
	CGameScene* gameScene = dynamic_cast< CGameScene* >( scene );

	/*if ( gameScene && ( m_nDrawOption == DRAW_SCENE_COLOR ) )
	{
		gameScene->RenderShadowMap(m_pd3dCommandList.Get(), m_GameTimer);
	}*/

	if ( scene && !gameScene )
		scene->OnPrepareRender(m_pd3dCommandList.Get(), m_pCamera);

	if ( m_nDrawOption == DRAW_SCENE_COLOR )
	{
		m_pd3dCommandList->ClearDepthStencilView(
			m_d3dDsvDescriptorCPUHandle,
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			1.0f,
			0,
			0,
			nullptr
		);

		if ( gameScene )
		{
			// 1) Geometry pass -> offscreen MRT only
			m_pPostProcessingShader->OnPrepareSceneRenderTargets(
				m_pd3dCommandList.Get(),
				&m_d3dDsvDescriptorCPUHandle
			);

			D3D12_CPU_DESCRIPTOR_HANDLE sceneRtvs[8] = {};
			UINT sceneRtvCount = 0;

			if ( m_pPostProcessingShader && m_pPostProcessingShader->GetTexture() )
			{
				sceneRtvCount = static_cast< UINT >(
					m_pPostProcessingShader->GetTexture()->GetTextures()
				);

				if ( sceneRtvCount > 8 )
					sceneRtvCount = 8;

				for ( UINT i = 0; i < sceneRtvCount; ++i )
					sceneRtvs[i] = m_pPostProcessingShader->GetRtvCPUDescriptorHandle(i);
			}

			gameScene->SetSceneRenderTargets(
				sceneRtvCount,
				sceneRtvs,
				m_d3dDsvDescriptorCPUHandle
			);

			{
				PROFILE_RENDER_SCOPE("Framework::GameScene::RenderShadowPrePass");
				gameScene->RenderShadowPrePass(m_pd3dCommandList.Get(), m_pCamera);
			}

			{
				PROFILE_RENDER_SCOPE("Framework::GameScene::RebindFrameForGeometry");
				gameScene->RebindFrameRenderState(m_pd3dCommandList.Get(), m_pCamera);
			}

			{
				PROFILE_RENDER_SCOPE("Framework::GameScene::RenderSceneGeometry");
				gameScene->RenderSceneGeometry(m_pd3dCommandList.Get(), m_pCamera);
			}

			m_pPostProcessingShader->OnPostRenderTarget(m_pd3dCommandList.Get());


			m_pd3dCommandList->ClearRenderTargetView(
				m_pd3dSwapChainBackBufferRTVCPUHandles[m_nSwapChainBufferIndex],
				Colors::SkyBlue,
				0,
				nullptr
			);

			m_pd3dCommandList->OMSetRenderTargets(1, &m_pd3dSwapChainBackBufferRTVCPUHandles[m_nSwapChainBufferIndex], FALSE, &m_d3dDsvDescriptorCPUHandle);

			{
				PROFILE_RENDER_SCOPE("Framework::GameScene::RebindFrameForComposite");
				gameScene->RebindFrameRenderState(m_pd3dCommandList.Get(), m_pCamera);
			}

			{
				PROFILE_RENDER_SCOPE("Framework::GameScene::RenderSceneComposite");
				gameScene->RenderSceneComposite(m_pd3dCommandList.Get(), m_pCamera);
			}
		}
		else
		{
			m_pd3dCommandList->ClearRenderTargetView(
				m_pd3dSwapChainBackBufferRTVCPUHandles[m_nSwapChainBufferIndex],
				Colors::SkyBlue,
				0,
				nullptr
			);

			m_pd3dCommandList->OMSetRenderTargets(
				1,
				&m_pd3dSwapChainBackBufferRTVCPUHandles[m_nSwapChainBufferIndex],
				FALSE,
				&m_d3dDsvDescriptorCPUHandle
			);

			if ( scene )
			{
				scene->OnPrepareRender(m_pd3dCommandList.Get(), m_pCamera);
				scene->Render(m_pd3dCommandList.Get(), m_pCamera);
			}
		}
	}
	else
	{
		m_pd3dCommandList->OMSetRenderTargets(
			1,
			&m_pd3dSwapChainBackBufferRTVCPUHandles[m_nSwapChainBufferIndex],
			TRUE,
			nullptr
		);

		m_pPostProcessingShader->Render(m_pd3dCommandList.Get(), m_pCamera, &m_nDrawOption);
	}

	::SynchronizeResourceTransition(
		m_pd3dCommandList.Get(),
		m_ppd3dSwapChainBackBuffers[m_nSwapChainBufferIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT
	);

	hResult = m_pd3dCommandList->Close();

	ID3D12CommandList* ppd3dCommandLists[ ] = { m_pd3dCommandList.Get() };
	m_pd3dCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);


	{
		PROFILE_RENDER_SCOPE("Framework::FrameAdvance::WaitForGpuComplete");
		WaitForGpuComplete();
	}

	{
		PROFILE_RENDER_SCOPE("Framework::FrameAdvance::Present");
		m_pdxgiSwapChain->Present(0, 0);
	}

	MoveToNextFrame();


	// 현재 프레임이 화면에 실제로 표시된 뒤,
	// 다음 프레임 시작에서 씬 전환이 가능하도록 만든다.
	if ( m_sceneSwitchPending && !m_sceneSwitchReadyToApply )
		m_sceneSwitchReadyToApply = true;

	m_GameTimer.GetFrameRate(m_pszFrameRate + 12, 37);
	::SetWindowText(m_hWnd, m_pszFrameRate);
}