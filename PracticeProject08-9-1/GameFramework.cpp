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

void CGameFramework::SetDisplayMode(DisplayMode DM, int Width, int Height)
{
	switch ( DM )
	{
	case DisplayMode::Windowed:

		break;
	case DisplayMode::BorderlessFullscreen:
		break;
	case DisplayMode::ExclusiveFullscreen:
		break;
	default:
		break;
	}
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
		music->RegisterMusic(EMusicState::Wait, "Assets/Audio/WaitSceneBGM.mp3");
		music->RegisterMusic(EMusicState::Gameplay, "Assets/Audio/ForestBGMWithBird.wav");
		music->SetCrossFadeSeconds(1.5f);
	}

	m_SceneManager.SetAudioManager(m_pAudioManager.get());

	BuildObjects();

	return( true );
}

void CGameFramework::OnDestroy()
{
	UnlockGameCursor();

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

	if ( m_pd3dCommandList )
		m_pd3dCommandList.Reset();

	if ( m_pd3dCommandAllocator )
		m_pd3dCommandAllocator.Reset();

	for ( UINT i = 0; i < m_nFrameContexts; ++i )
	{
		if ( m_frameContexts[i].commandList )
			m_frameContexts[i].commandList.Reset();

		if ( m_frameContexts[i].commandAllocator )
			m_frameContexts[i].commandAllocator.Reset();

		m_frameContexts[i].fenceValue = 0;
	}

	if ( m_pd3dCommandQueue )
		m_pd3dCommandQueue.Reset();

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
	FlushGpu();

	m_SceneManager.ReleaseCurrent();
	m_pCamera = nullptr;

	if ( m_pPostProcessingShader )
	{
		m_pPostProcessingShader->ReleaseObjects();
		m_pPostProcessingShader.reset();
	}
}

UINT64 CGameFramework::SignalCommandQueue()
{
	const UINT64 fenceValue = m_nNextFenceValue++;

	HRESULT hResult = m_pd3dCommandQueue->Signal(
		m_pd3dFence.Get(),
		fenceValue
	);
	( void ) hResult;

	return fenceValue;
}

void CGameFramework::WaitForFenceValue(UINT64 fenceValue)
{
	if ( fenceValue == 0 )
		return;

	if ( m_pd3dFence->GetCompletedValue() < fenceValue )
	{
		HRESULT hResult = m_pd3dFence->SetEventOnCompletion(
			fenceValue,
			m_hFenceEvent
		);
		( void ) hResult;

		::WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}

void CGameFramework::WaitForFrameContext(UINT frameContextIndex)
{
	if ( frameContextIndex >= m_nFrameContexts )
		return;

	WaitForFenceValue(m_frameContexts[frameContextIndex].fenceValue);
}

void CGameFramework::FlushGpu()
{
	PROFILE_RENDER_SCOPE("Framework::FlushGpu(total)");

	const UINT64 fenceValue = SignalCommandQueue();
	WaitForFenceValue(fenceValue);
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

	if ( FAILED(hResult) )
		return;

	for ( UINT i = 0; ; ++i )
	{
		// 1. 고성능 GPU 어댑터 생성
		hResult = m_pdxgiFactory->EnumAdapterByGpuPreference(
			i,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&m_pd3dGPUAdapter));

		if ( hResult == DXGI_ERROR_NOT_FOUND )
			break;

		if ( FAILED(hResult) )
			continue;
	
		DXGI_ADAPTER_DESC1 desc = {};
		m_pd3dGPUAdapter->GetDesc1(&desc);

		if ( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE )
			continue;

		hResult = D3D12CreateDevice(
			m_pd3dGPUAdapter.Get(),
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&m_pd3dDevice));

		if ( SUCCEEDED(hResult) )
			break;
	}
	
	if (!m_pd3dDevice)
	{
		hResult = m_pdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&m_pd3dGPUAdapter));
		hResult = D3D12CreateDevice(
			m_pd3dGPUAdapter.Get(),
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

	m_nNextFenceValue = 1;

	m_hFenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

	FindOutputForCurrentWindow();

	::gnRtvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	::gnCbvSrvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void CGameFramework::FindOutputForCurrentWindow()
{
	HMONITOR targetMonitor = ::MonitorFromWindow(
		m_hWnd,
		MONITOR_DEFAULTTONEAREST
	);

	if (!targetMonitor)
		return;

	if ( m_OutputDesc.Monitor == targetMonitor && !m_DisplayModeList.empty())
		return;

	ComPtr<IDXGIOutput> pOutput;
	UINT nModes = 0;
	UINT nDXGIOutputFlags = DXGI_ENUM_MODES_INTERLACED/*0*/;

	for (UINT outputIndex = 0; ; ++outputIndex)
	{

		HRESULT hr = m_pd3dGPUAdapter->EnumOutputs(outputIndex, &pOutput);

		if (hr == DXGI_ERROR_NOT_FOUND)
			break;

		if (FAILED(hr) || !pOutput)
			continue;

		hr = pOutput->GetDesc(&m_OutputDesc);

		if (FAILED(hr))
			continue;

		if (m_OutputDesc.Monitor == targetMonitor)
		{
			m_bHasGpuOutput = true;

			hr = pOutput->GetDisplayModeList(
				DXGI_FORMAT_R8G8B8A8_UNORM,
				nDXGIOutputFlags,
				&nModes,
				nullptr
			);

			if (FAILED(hr) || nModes == 0)
				return;

			m_DisplayModeList.resize(nModes);

			hr = pOutput->GetDisplayModeList(
				DXGI_FORMAT_R8G8B8A8_UNORM,
				nDXGIOutputFlags,
				&nModes,
				m_DisplayModeList.data()
			);

			return;
		}
	}

	for (UINT adapterIndex = 0; ; ++adapterIndex)
	{
		ComPtr<IDXGIAdapter1> adapter;

		HRESULT hr = m_pdxgiFactory->EnumAdapters1(adapterIndex, &adapter);
		if (hr == DXGI_ERROR_NOT_FOUND)
			break;

		if (FAILED(hr) || !adapter)
			continue;

		DXGI_ADAPTER_DESC1 adapterDesc{};
		adapter->GetDesc1(&adapterDesc);

		if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		for (UINT outputIndex = 0; ; ++outputIndex)
		{
			hr = adapter->EnumOutputs(outputIndex, &pOutput);
			if (hr == DXGI_ERROR_NOT_FOUND)
				break;

			if (FAILED(hr) || !pOutput)
				continue;

			hr = pOutput->GetDesc(&m_OutputDesc);
			if (FAILED(hr))
				continue;

			if (m_OutputDesc.Monitor == targetMonitor)
			{
				m_bHasGpuOutput = false;

				hr = pOutput->GetDisplayModeList(
					DXGI_FORMAT_R8G8B8A8_UNORM,
					nDXGIOutputFlags,
					&nModes,
					nullptr
				);

				if (FAILED(hr) || nModes == 0)
					return;

				m_DisplayModeList.resize(nModes);

				hr = pOutput->GetDisplayModeList(
					DXGI_FORMAT_R8G8B8A8_UNORM,
					nDXGIOutputFlags,
					&nModes,
					m_DisplayModeList.data()
				);

				return;
			}
		}
	}

	return;
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
	( void ) hResult;

	for ( UINT i = 0; i < m_nFrameContexts; ++i )
	{
		hResult = m_pd3dDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&m_frameContexts[i].commandAllocator));
		( void ) hResult;

		hResult = m_pd3dDevice->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_frameContexts[i].commandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(&m_frameContexts[i].commandList));
		( void ) hResult;

		hResult = m_frameContexts[i].commandList->Close();
		( void ) hResult;

		m_frameContexts[i].fenceValue = 0;
	}

	m_nFrameContextIndex = 0;

	m_pd3dCommandAllocator = m_frameContexts[m_nFrameContextIndex].commandAllocator;
	m_pd3dCommandList = m_frameContexts[m_nFrameContextIndex].commandList;
}

void CGameFramework::CreateRtvAndDsvDescriptorHeaps()
{
	HRESULT hResult;

	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
	::ZeroMemory(&d3dDescriptorHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	d3dDescriptorHeapDesc.NumDescriptors = m_nSwapChainBuffers + 8;
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
	UpdateGameCursorLock();
}

bool CGameFramework::HandlePauseClick(UINT nMessageID, LPARAM lParam)
{
	if ( ( nMessageID != WM_LBUTTONDOWN ) && ( nMessageID != WM_RBUTTONDOWN ) )
		return false;

	if ( !IsInputPauseActive() )
		return false;

	CGameScene* gameScene = dynamic_cast< CGameScene* >( m_SceneManager.GetScene() );
	if ( !gameScene )
		return true;

	POINT ptClient = {};
	ptClient.x = GET_X_LPARAM(lParam);
	ptClient.y = GET_Y_LPARAM(lParam);

	if ( gameScene->IsPointInResumeButton(ptClient) )
	{
		ClearInputPause();
		return true;
	}

	if ( gameScene->IsPointInExitButton(ptClient) )
	{
		g_End.store(true);
		::PostQuitMessage(0);
		return true;
	}

	// Pause 배경 또는 빈 공간 클릭은 아무 일도 하지 않고 입력만 소비한다.
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

bool CGameFramework::IsGameSceneActive() const
{
	return dynamic_cast< CGameScene* >( m_SceneManager.GetScene() ) != nullptr;
}

bool CGameFramework::ShouldLockGameCursor() const
{
	return
		m_hWnd != nullptr &&
		m_bWindowActive &&
		IsGameSceneActive() &&
		!IsInputPauseActive();
}

POINT CGameFramework::GetClientCenterScreenPoint() const
{
	POINT center{};

	if ( !m_hWnd )
		return center;

	RECT rc{};
	::GetClientRect(m_hWnd, &rc);

	center.x = ( rc.left + rc.right ) / 2;
	center.y = ( rc.top + rc.bottom ) / 2;

	::ClientToScreen(m_hWnd, &center);
	return center;
}

void CGameFramework::LockGameCursor()
{
	if ( !m_hWnd )
		return;

	RECT rc{};
	::GetClientRect(m_hWnd, &rc);

	POINT lt{ rc.left, rc.top };
	POINT rb{ rc.right, rc.bottom };

	::ClientToScreen(m_hWnd, &lt);
	::ClientToScreen(m_hWnd, &rb);

	RECT clipRect{};
	clipRect.left = lt.x;
	clipRect.top = lt.y;
	clipRect.right = rb.x;
	clipRect.bottom = rb.y;

	::ClipCursor(&clipRect);
	::SetCapture(m_hWnd);

	while ( ::ShowCursor(FALSE) >= 0 )
	{
	}

	m_ptOldCursorPos = GetClientCenterScreenPoint();
	::SetCursorPos(m_ptOldCursorPos.x, m_ptOldCursorPos.y);

	m_bGameCursorLocked = true;
}

void CGameFramework::UnlockGameCursor()
{
	::ClipCursor(nullptr);

	if ( ::GetCapture() == m_hWnd )
		::ReleaseCapture();

	while ( ::ShowCursor(TRUE) < 0 )
	{
	}

	::GetCursorPos(&m_ptOldCursorPos);

	m_bGameCursorLocked = false;
}

void CGameFramework::UpdateGameCursorLock()
{
	const bool shouldLock = ShouldLockGameCursor();

	if ( shouldLock )
	{
		if ( !m_bGameCursorLocked )
		{
			LockGameCursor();
			return;
		}

		// 창 이동/크기 변경에 대비해서 clip rect를 계속 최신화.
		RECT rc{};
		::GetClientRect(m_hWnd, &rc);

		POINT lt{ rc.left, rc.top };
		POINT rb{ rc.right, rc.bottom };

		::ClientToScreen(m_hWnd, &lt);
		::ClientToScreen(m_hWnd, &rb);

		RECT clipRect{};
		clipRect.left = lt.x;
		clipRect.top = lt.y;
		clipRect.right = rb.x;
		clipRect.bottom = rb.y;

		::ClipCursor(&clipRect);
		return;
	}

	if ( m_bGameCursorLocked )
		UnlockGameCursor();
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

	if ( !m_bWindowActive )
	{
		UnlockGameCursor();

		// 비활성화 = ESC pause와 동일하게 처리
		if ( dynamic_cast< CGameScene* >( m_SceneManager.GetScene() ) )
			m_bUserPaused = true;

		SyncGameSceneInactiveOverlay();
		UpdateGameCursorLock();
	}
	else
	{
		::GetCursorPos(&m_ptOldCursorPos);

		// 다시 클릭해서 활성화될 때 첫 클릭은
		// 오직 활성화만 하고 어떤 pause 처리도 하지 않음
		m_bConsumeNextMouseClick = true;

		SyncGameSceneInactiveOverlay();
		UpdateGameCursorLock();
	}
}

void CGameFramework::BuildSceneInternal(ESceneId id, bool resetTimer)
{
	( void ) resetTimer;

	FlushGpu();

	for ( UINT i = 0; i < m_nFrameContexts; ++i )
		m_frameContexts[i].fenceValue = 0;

	m_SceneManager.ReleaseCurrent();
	m_pCamera = nullptr;

	if ( m_pPostProcessingShader )
	{
		m_pPostProcessingShader->ReleaseObjects();
		m_pPostProcessingShader.reset();
	}

	WaitForFrameContext(m_nFrameContextIndex);

	FrameContext& frame = m_frameContexts[m_nFrameContextIndex];

	m_pd3dCommandAllocator = frame.commandAllocator;
	m_pd3dCommandList = frame.commandList;

	HRESULT hr = m_pd3dCommandAllocator->Reset();
	( void ) hr;

	hr = m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), nullptr);
	( void ) hr;

	m_SceneManager.BuildScene(id, m_pd3dDevice.Get(), m_pd3dCommandList.Get());

	CScene* scene = m_SceneManager.GetScene();
	if ( !scene )
	{
		hr = m_pd3dCommandList->Close();
		( void ) hr;

		ID3D12CommandList* ppd3dCommandLists[ ] = { m_pd3dCommandList.Get() };
		m_pd3dCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

		frame.fenceValue = SignalCommandQueue();
		WaitForFenceValue(frame.fenceValue);
		frame.fenceValue = 0;

		m_GameTimer.Reset();
		return;
	}

	m_bUserPaused = false;
	m_bConsumeNextMouseClick = false;

	m_pCamera = scene->GetMainCamera();

	if ( m_pCamera )
		m_pCamera->SetFrameResourceIndex(m_nFrameContextIndex);

	SyncGameSceneInactiveOverlay();
	UpdateGameCursorLock();

	m_pPostProcessingShader = make_shared<CTextureToFullScreenShader>();
	m_pPostProcessingShader->CreateShader(
		m_pd3dDevice.Get(),
		scene->GetGraphicsRootSignature(),
		1,
		nullptr,
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);

	m_pPostProcessingShader->BuildObjects(
		m_pd3dDevice.Get(),
		m_pd3dCommandList.Get(),
		&m_nDrawOption
	);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	d3dRtvCPUDescriptorHandle.ptr +=
		( ::gnRtvDescriptorIncrementSize * m_nSwapChainBuffers );

	DXGI_FORMAT pdxgiResourceFormats[5] =
	{
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R32_FLOAT
	};

	m_pPostProcessingShader->CreateResourcesAndRtvsSrvs(
		m_pd3dDevice.Get(),
		m_pd3dCommandList.Get(),
		5,
		pdxgiResourceFormats,
		d3dRtvCPUDescriptorHandle,
		static_cast<UINT>(m_nWndClientWidth),
		static_cast<UINT>(m_nWndClientHeight)
	);

	if ( CGameScene* gameScene = dynamic_cast< CGameScene* >( scene ) )
	{
		gameScene->SetFrameResourceIndex(m_nFrameContextIndex);
		gameScene->SetDepthFogSourceSrvIndices(
			m_pPostProcessingShader->GetTexture()->GetSrvIndex(0),
			m_pPostProcessingShader->GetTexture()->GetSrvIndex(3),
			m_pPostProcessingShader->GetTexture()->GetSrvIndex(4)
		);

		D3D12_CPU_DESCRIPTOR_HANDLE sceneRtvs[8] = {};
		D3D12_CPU_DESCRIPTOR_HANDLE sceneRtvHandle =
			m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		sceneRtvHandle.ptr +=
			( ::gnRtvDescriptorIncrementSize * m_nSwapChainBuffers );

		for ( UINT i = 0; i < 8; ++i )
		{
			sceneRtvs[i] = sceneRtvHandle;
			sceneRtvHandle.ptr += ::gnRtvDescriptorIncrementSize;
		}

		gameScene->SetSceneRenderTargets(
			8,
			sceneRtvs,
			m_d3dDsvDescriptorCPUHandle
		);
	}

	hr = m_pd3dCommandList->Close();
	( void ) hr;

	ID3D12CommandList* ppd3dCommandLists[ ] = { m_pd3dCommandList.Get() };
	m_pd3dCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	frame.fenceValue = SignalCommandQueue();
	WaitForFenceValue(frame.fenceValue);
	frame.fenceValue = 0;

	scene->ReleaseUploadBuffers();

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
			if ( req == CScene::ESceneRequest::SwitchToWait ) RequestSceneSwitch(ESceneId::Wait, false);
			else if ( req == CScene::ESceneRequest::SwitchToGame ) RequestSceneSwitch(ESceneId::Game, true);
		}
	}

	switch ( nMessageID )
	{
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
		if ( !m_bGameCursorLocked )
		{
			::SetCapture(hWnd);
			::GetCursorPos(&m_ptOldCursorPos);
		}
		break;

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		if ( !m_bGameCursorLocked )
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
		if ( wParam == VK_ESCAPE )
		{
			if ( dynamic_cast< CGameScene* >( m_SceneManager.GetScene() ) )
			{
				m_bUserPaused = !m_bUserPaused;
				SyncGameSceneInactiveOverlay();
				UpdateGameCursorLock();
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
	FlushGpu();

	FindOutputForCurrentWindow();

	if (m_DisplayMode == DisplayMode::Windowed)
	{
		if (m_bHasGpuOutput)
		{
			HRESULT hr = m_pdxgiSwapChain->SetFullscreenState(TRUE, nullptr);

			if (SUCCEEDED(hr))
			{
				m_DisplayMode = DisplayMode::ExclusiveFullscreen;

				m_nWndClientWidth = m_OutputDesc.DesktopCoordinates.right - m_OutputDesc.DesktopCoordinates.left;
				m_nWndClientHeight = m_OutputDesc.DesktopCoordinates.bottom - m_OutputDesc.DesktopCoordinates.top;
				
				OnResize(m_nWndClientWidth, m_nWndClientHeight);
				return;
			}
		}
		else
		{
			EnterBorderlessFullscreen();
			return;
		}
		
	}
	else if (m_DisplayMode == DisplayMode::ExclusiveFullscreen)
	{
		m_pdxgiSwapChain->SetFullscreenState(FALSE, nullptr);
		m_DisplayMode = DisplayMode::Windowed;
		OnResize(FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
		return;
	}
	else if (m_DisplayMode == DisplayMode::BorderlessFullscreen)
	{
		m_DisplayMode = DisplayMode::Windowed;
		LeaveBorderlessFullscreen();
		return;
	}
}

void CGameFramework::OnResize(int width, int height)
{
	if (!m_pd3dDevice || !m_pdxgiSwapChain)
		return;

	if (width <= 0 || height <= 0)
		return;

	m_nWndClientWidth = width;
	m_nWndClientHeight = height;

	FlushGpu();

	DXGI_MODE_DESC dxgiTargetParameters;
	::ZeroMemory(&dxgiTargetParameters, sizeof(DXGI_MODE_DESC));
	dxgiTargetParameters.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiTargetParameters.Width = width;
	dxgiTargetParameters.Height = height;
	dxgiTargetParameters.RefreshRate.Numerator = 60;
	dxgiTargetParameters.RefreshRate.Denominator = 1;
	dxgiTargetParameters.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	dxgiTargetParameters.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

	HRESULT hResult = m_pdxgiSwapChain->ResizeTarget(&dxgiTargetParameters);
	( void ) hResult;
	
	for ( int i = 0; i < m_nSwapChainBuffers; ++i )
	{
		if ( m_ppd3dSwapChainBackBuffers[i] )
			m_ppd3dSwapChainBackBuffers[i].Reset();
	}

	if (m_pd3dDepthStencilBuffer)
		m_pd3dDepthStencilBuffer.Reset();

	DXGI_SWAP_CHAIN_DESC dxgiSwapChainDesc;
	::ZeroMemory(&dxgiSwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

	hResult = m_pdxgiSwapChain->GetDesc(&dxgiSwapChainDesc);
	( void ) hResult;

	hResult = m_pdxgiSwapChain->ResizeBuffers(
		m_nSwapChainBuffers,
		width,
		height,
		dxgiSwapChainDesc.BufferDesc.Format,
		dxgiSwapChainDesc.Flags
	);
	( void ) hResult;

	m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();

	CreateSwapChainRenderTargetViews();
	CreateDepthStencilView();

	if (m_pPostProcessingShader)
	{
		m_pPostProcessingShader->ReleaseShaderVariables();

		D3D12_CPU_DESCRIPTOR_HANDLE postProcessRtvHandle =
			m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

		postProcessRtvHandle.ptr +=
			(::gnRtvDescriptorIncrementSize * m_nSwapChainBuffers);

		DXGI_FORMAT postProcessFormats[5] =
		{
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_R32_FLOAT
		};

		m_pPostProcessingShader->CreateResourcesAndRtvsSrvs(
			m_pd3dDevice.Get(),
			m_pd3dCommandList.Get(),
			5,
			postProcessFormats,
			postProcessRtvHandle,
			static_cast<UINT>(width),
			static_cast<UINT>(height)
		);
	}

	CScene* scene = m_SceneManager.GetScene();

	if ( CGameScene* gameScene = dynamic_cast< CGameScene* >( scene ) )
	{
		if ( m_pPostProcessingShader && m_pPostProcessingShader->GetTexture() )
		{
			gameScene->SetDepthFogSourceSrvIndices(
				m_pPostProcessingShader->GetTexture()->GetSrvIndex(0),
				m_pPostProcessingShader->GetTexture()->GetSrvIndex(3),
				m_pPostProcessingShader->GetTexture()->GetSrvIndex(4)
			);
		}

		D3D12_CPU_DESCRIPTOR_HANDLE sceneRtvs[8] = {};
		D3D12_CPU_DESCRIPTOR_HANDLE sceneRtvHandle =
			m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		sceneRtvHandle.ptr +=
			( ::gnRtvDescriptorIncrementSize * m_nSwapChainBuffers );

		for ( UINT i = 0; i < 8; ++i )
		{
			sceneRtvs[i] = sceneRtvHandle;
			sceneRtvHandle.ptr += ::gnRtvDescriptorIncrementSize;
		}

		gameScene->SetSceneRenderTargets(
			8,
			sceneRtvs,
			m_d3dDsvDescriptorCPUHandle
		);
	}

	if (scene)
		scene->OnResize(width, height);

	m_pCamera = scene ? scene->GetMainCamera() : nullptr;

	if (m_pCamera)
		m_pCamera->SetFrameResourceIndex(m_nFrameContextIndex);

	for ( UINT i = 0; i < m_nFrameContexts; ++i )
		m_frameContexts[i].fenceValue = 0;

	m_nFrameContextIndex = 0;
	m_pd3dCommandAllocator = m_frameContexts[m_nFrameContextIndex].commandAllocator;
	m_pd3dCommandList = m_frameContexts[m_nFrameContextIndex].commandList;
}

void CGameFramework::EnterBorderlessFullscreen()
{
	// 1. 기존 창 스타일 저장
	m_dwWindowedStyle = GetWindowLong(m_hWnd, GWL_STYLE);

	// 2. 기존 창 위치/크기/상태 저장
	m_WindowedPlacement.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(m_hWnd, &m_WindowedPlacement);

	// 3. 현재 창이 위치한 모니터 정보 얻기
	MONITORINFO mi{};
	mi.cbSize = sizeof(MONITORINFO);

	HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
	GetMonitorInfo(hMonitor, &mi);

	const int width = mi.rcMonitor.right - mi.rcMonitor.left;
	const int height = mi.rcMonitor.bottom - mi.rcMonitor.top;

	// 4. 창 스타일에서 테두리/타이틀바 제거
	SetWindowLong(
		m_hWnd,
		GWL_STYLE,
		m_dwWindowedStyle & ~WS_OVERLAPPEDWINDOW
	);

	// 5. 창을 모니터 전체 영역으로 이동/확대
	SetWindowPos(
		m_hWnd,
		HWND_TOP,
		mi.rcMonitor.left,
		mi.rcMonitor.top,
		width,
		height,
		SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_SHOWWINDOW
	);

	// 6. 내부 상태 갱신
	m_DisplayMode = DisplayMode::BorderlessFullscreen;

	// 7. 렌더 타겟/백버퍼 크기 갱신
	OnResize(width, height);
}

void CGameFramework::LeaveBorderlessFullscreen()
{
	SetWindowLong(m_hWnd, GWL_STYLE, m_dwWindowedStyle | WS_OVERLAPPEDWINDOW);
	SetWindowPlacement(m_hWnd, &m_WindowedPlacement);

	SetWindowPos(
		m_hWnd,
		nullptr,
		0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED
	);

	OnResize(FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
}

bool CGameFramework::CanUseExclusiveFullscreen() const
{
	if (!m_pd3dGPUAdapter || !m_hWnd)
		return false;

	HMONITOR targetMonitor = MonitorFromWindow(
		m_hWnd,
		MONITOR_DEFAULTTONEAREST
	);

	if (!targetMonitor)
		return false;

	for (UINT i = 0; ; ++i)
	{
		ComPtr<IDXGIOutput> output;
		HRESULT hr = m_pd3dGPUAdapter->EnumOutputs(i, &output);

		if (hr == DXGI_ERROR_NOT_FOUND)
			break;

		if (FAILED(hr) || !output)
			continue;

		DXGI_OUTPUT_DESC desc{};
		if (FAILED(output->GetDesc(&desc)))
			continue;

		if (desc.Monitor == targetMonitor)
			return true;
	}

	return false;
}

void CGameFramework::ProcessInput()
{
	UpdateWindowActivationState();
	UpdateGameCursorLock();

	if ( IsInputPauseActive() )
		return;

	static UCHAR pKeysBuffer[256];
	bool bProcessedByScene = false;

	CScene* scene = m_SceneManager.GetScene();
	if ( GetKeyboardState(pKeysBuffer) && scene )
		bProcessedByScene = scene->ProcessInput(pKeysBuffer);

	// Demo: U/I/O/P -> Player slot(0/1/2/3) Attack (edge trigger)
	static bool s_prevDemoAttackDown[4] = { false, false, false, false };
	static constexpr int kDemoAttackKeys[4] =
	{
		'U', // slot 0
		'I', // slot 1
		'O', // slot 2
		'P'  // slot 3
	};

	for ( int slot = 0; slot < 4; ++slot )
	{
		const bool down = ( pKeysBuffer[kDemoAttackKeys[slot]] & 0xF0 ) != 0;

		if ( down && !s_prevDemoAttackDown[slot] )
		{
			if ( scene )
				scene->RequestPlayerAttackBySlot(slot);
		}

		s_prevDemoAttackDown[slot] = down;
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

		if ( pKeysBuffer['1'] & 0xF0 ) keyCodes |= ( 1 << 10 ); // Inventory slot 0 use request
		if ( pKeysBuffer['2'] & 0xF0 ) keyCodes |= ( 1 << 11 ); // Inventory slot 1 use request
		if ( pKeysBuffer['3'] & 0xF0 ) keyCodes |= ( 1 << 12 ); // Inventory slot 2 use request
		if ( pKeysBuffer['4'] & 0xF0 ) keyCodes |= ( 1 << 13 ); // Inventory slot 3 use request

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

		const bool shiftDown =
			( ( pKeysBuffer[VK_LSHIFT] & 0xF0 ) != 0 ) ||
			( ( pKeysBuffer[VK_SHIFT] & 0xF0 ) != 0 );

		const DWORD horizontalDirBits =
			dwDirection & ( DIR_FORWARD | DIR_BACKWARD | DIR_LEFT | DIR_RIGHT );

		bRunRequested = shiftDown && ( horizontalDirBits != 0 );

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

		if ( m_bGameCursorLocked )
		{
			const POINT center = GetClientCenterScreenPoint();

			::GetCursorPos(&ptCursorPos);

			cxDelta = ( float ) ( ptCursorPos.x - center.x ) / 3.0f;
			cyDelta = ( float ) ( ptCursorPos.y - center.y ) / 3.0f;

			::SetCursorPos(center.x, center.y);
			m_ptOldCursorPos = center;
		}
		else
		{
			::GetCursorPos(&ptCursorPos);

			cxDelta = ( float ) ( ptCursorPos.x - m_ptOldCursorPos.x ) / 3.0f;
			cyDelta = ( float ) ( ptCursorPos.y - m_ptOldCursorPos.y ) / 3.0f;

			m_ptOldCursorPos = ptCursorPos;
		}

#ifdef USING_NETWORK
		inputPkt.set_deltax(cxDelta);
		inputPkt.set_deltay(cyDelta);

		float dt = m_GameTimer.GetTimeElapsed();
		if (dt < 0.001f) dt = 0.001f;
		if (dt > 0.05f)  dt = 0.05f;
		inputPkt.set_clientdeltatime(dt);

		auto sendBuffer = ServerPacketHandler::MakeSendBuffer(inputPkt);
		g_clientService->BroadCast(sendBuffer);

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

		pc->SetInputDirection(static_cast< uint32_t >( dwDirection ));

		if ( dwDirection && !pc->IsActionLockedByAnimation() )
		{
			// Network mode: local prediction movement is disabled; server snapshots drive position.
			const XMFLOAT3 prevPos = playerObj->GetPosition();

			const float moveSpeed = pc->GetCurrentMoveSpeed();
			pc->MoveByYaw(dwDirection, moveSpeed * dt, cameraYawDeg, false);

			if ( CGameScene* gameScene = dynamic_cast< CGameScene* >( scene ) )
			{
				gameScene->ApplyNetworkPredictedTerrainY(playerObj);
				gameScene->RollbackLocalPlayerMoveIfCollidingWorldStatic(prevPos);
			}
		}

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

		pc->SetInputDirection(static_cast< uint32_t >( dwDirection ));

		if ( dwDirection && !pc->IsActionLockedByAnimation() )
		{
			const XMFLOAT3 prevPos = playerObj->GetPosition();

			const float moveSpeed = pc->GetCurrentMoveSpeed();
			pc->MoveByYaw(dwDirection, moveSpeed * dt, cameraYawDeg, false);

			if ( CGameScene* gameScene = dynamic_cast< CGameScene* >( scene ) )
			{
				gameScene->RollbackLocalPlayerMoveIfCollidingWorldStatic(prevPos);
			}
		}

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
	m_nFrameContextIndex = ( m_nFrameContextIndex + 1 ) % m_nFrameContexts;
	m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();

	m_pd3dCommandAllocator = m_frameContexts[m_nFrameContextIndex].commandAllocator;
	m_pd3dCommandList = m_frameContexts[m_nFrameContextIndex].commandList;
}

void CGameFramework::FrameAdvance()
{
	PROFILE_RENDER_SCOPE("Framework::FrameAdvance(total)");

	HRESULT hResult;


	m_GameTimer.Tick(0.0f);
	UpdateWindowActivationState();
	UpdateGameCursorLock();

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
	AnimateObjects();
	

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
	CollisionSystem();

	{
		PROFILE_RENDER_SCOPE("Framework::WaitForFrameContext");
		WaitForFrameContext(m_nFrameContextIndex);
	}

	m_pd3dCommandAllocator = m_frameContexts[m_nFrameContextIndex].commandAllocator;
	m_pd3dCommandList = m_frameContexts[m_nFrameContextIndex].commandList;

	hResult = m_pd3dCommandAllocator->Reset();
	( void ) hResult;

	hResult = m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), nullptr);
	( void ) hResult;

	if ( m_pCamera )
		m_pCamera->SetFrameResourceIndex(m_nFrameContextIndex);

	::SynchronizeResourceTransition(
		m_pd3dCommandList.Get(),
		m_ppd3dSwapChainBackBuffers[m_nSwapChainBufferIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	CScene* scene = m_SceneManager.GetScene();
	CGameScene* gameScene = dynamic_cast< CGameScene* >( scene );

	if ( gameScene )
		gameScene->SetFrameResourceIndex(m_nFrameContextIndex);

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

				if ( sceneRtvCount > 5 )
					sceneRtvCount = 5;

				for ( UINT i = 0; i < sceneRtvCount; ++i )
					sceneRtvs[i] = m_pPostProcessingShader->GetRtvCPUDescriptorHandle(i);
			}

			D3D12_CPU_DESCRIPTOR_HANDLE ssaoRtvHandle =
				m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			ssaoRtvHandle.ptr +=
				( ::gnRtvDescriptorIncrementSize * ( m_nSwapChainBuffers + 5 ) );

			for ( UINT i = 5; i < 8; ++i )
			{
				sceneRtvs[i] = ssaoRtvHandle;
				ssaoRtvHandle.ptr += ::gnRtvDescriptorIncrementSize;
			}

			gameScene->SetSceneRenderTargets(
				8,
				sceneRtvs,
				m_d3dDsvDescriptorCPUHandle
			);

			
			gameScene->RenderShadowPrePass(m_pd3dCommandList.Get(), m_pCamera);
			gameScene->RebindFrameRenderState(m_pd3dCommandList.Get(), m_pCamera);
			gameScene->RenderSceneGeometry(m_pd3dCommandList.Get(), m_pCamera);

			if ( m_pPostProcessingShader && m_pPostProcessingShader->GetTexture() )
			{
				::SynchronizeResourceTransition(
					m_pd3dCommandList.Get(),
					m_pPostProcessingShader->GetTextureResource(3),
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_GENERIC_READ
				);
				::SynchronizeResourceTransition(
					m_pd3dCommandList.Get(),
					m_pPostProcessingShader->GetTextureResource(4),
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_GENERIC_READ
				);
			}

			gameScene->RenderSsao(m_pd3dCommandList.Get(), m_pCamera);

			if ( m_pPostProcessingShader && m_pPostProcessingShader->GetTexture() )
			{
				::SynchronizeResourceTransition(
					m_pd3dCommandList.Get(),
					m_pPostProcessingShader->GetTextureResource(3),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					D3D12_RESOURCE_STATE_RENDER_TARGET
				);
				::SynchronizeResourceTransition(
					m_pd3dCommandList.Get(),
					m_pPostProcessingShader->GetTextureResource(4),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					D3D12_RESOURCE_STATE_RENDER_TARGET
				);
			}
			

			m_pPostProcessingShader->OnPostRenderTarget(m_pd3dCommandList.Get());


			m_pd3dCommandList->ClearRenderTargetView(
				m_pd3dSwapChainBackBufferRTVCPUHandles[m_nSwapChainBufferIndex],
				Colors::SkyBlue,
				0,
				nullptr
			);

			m_pd3dCommandList->OMSetRenderTargets(1, &m_pd3dSwapChainBackBufferRTVCPUHandles[m_nSwapChainBufferIndex], FALSE, &m_d3dDsvDescriptorCPUHandle);
			gameScene->RebindFrameRenderState(m_pd3dCommandList.Get(), m_pCamera);
			
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
	( void ) hResult;

	ID3D12CommandList* ppd3dCommandLists[ ] = { m_pd3dCommandList.Get() };
	m_pd3dCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	m_frameContexts[m_nFrameContextIndex].fenceValue = SignalCommandQueue();

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
