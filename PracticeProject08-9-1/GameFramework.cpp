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

#include "Service.h"
#include "ServerPacketHandler.h"
#include "GlobalValues.h"


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
		GLOBAL_SRV_CAPACITY
	);

	BuildObjects();

	return(true);
}

void CGameFramework::OnDestroy()
{
	ReleaseObjects();

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
	const UINT64 nFenceValue = ++m_nFenceValues[m_nSwapChainBufferIndex];
	HRESULT hResult = m_pd3dCommandQueue->Signal(m_pd3dFence.Get(), nFenceValue);

	if (m_pd3dFence->GetCompletedValue() < nFenceValue)
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
		IID_PPV_ARGS(&m_pdxgiFactory)
	);

	ComPtr<IDXGIAdapter1> pd3dAdapter;

	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != m_pdxgiFactory->EnumAdapters1(i, &pd3dAdapter); i++)
	{
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
		pd3dAdapter->GetDesc1(&dxgiAdapterDesc);
		if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		hResult = D3D12CreateDevice(
			pd3dAdapter.Get(),
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&m_pd3dDevice)
		);

		if (SUCCEEDED(hResult))
			break;
	}

	if (!m_pd3dDevice)
	{
		hResult = m_pdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pd3dAdapter));
		hResult = D3D12CreateDevice(
			pd3dAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_pd3dDevice)
		);
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
		sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS)
	);
	m_nMsaa4xQualityLevels = d3dMsaaQualityLevels.NumQualityLevels;
	m_bMsaa4xEnable = (m_nMsaa4xQualityLevels > 1) ? true : false;

	hResult = m_pd3dDevice->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_pd3dFence)
	);

	for (UINT i = 0; i < m_nSwapChainBuffers; i++)
		m_nFenceValues[i] = 1;

	m_hFenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

	::gnRtvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	::gnCbvSrvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void CGameFramework::CreateCommandQueueAndList()
{
	HRESULT hResult;

	D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
	::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hResult = m_pd3dDevice->CreateCommandQueue(
		&d3dCommandQueueDesc,
		IID_PPV_ARGS(&m_pd3dCommandQueue)
	);

	hResult = m_pd3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&m_pd3dCommandAllocator)
	);

	hResult = m_pd3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_pd3dCommandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&m_pd3dCommandList)
	);

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
		IID_PPV_ARGS(&m_pd3dRtvDescriptorHeap)
	);

	d3dDescriptorHeapDesc.NumDescriptors = 1;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

	hResult = m_pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(&m_pd3dDsvDescriptorHeap)
	);
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
		(IDXGISwapChain**)m_pdxgiSwapChain.ReleaseAndGetAddressOf()
	);

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
			IID_PPV_ARGS(m_ppd3dSwapChainBackBuffers[i].ReleaseAndGetAddressOf())
		);

		m_pd3dDevice->CreateRenderTargetView(
			m_ppd3dSwapChainBackBuffers[i].Get(),
			&d3dRenderTargetViewDesc,
			d3dRtvCPUDescriptorHandle
		);

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
	if (!scene) return;

	CGameScene* gameScene = dynamic_cast<CGameScene*>(scene);
	if (!gameScene) return;

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
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);
	m_pPostProcessingShader->BuildObjects(m_pd3dDevice.Get(), m_pd3dCommandList.Get(), &m_nDrawOption);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (::gnRtvDescriptorIncrementSize * m_nSwapChainBuffers);

	DXGI_FORMAT pdxgiResourceFormats[4] = {
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R32_FLOAT
	};

	m_pPostProcessingShader->CreateResourcesAndRtvsSrvs(
		m_pd3dDevice.Get(),
		m_pd3dCommandList.Get(),
		4,
		pdxgiResourceFormats,
		d3dRtvCPUDescriptorHandle
	);

	D3D12_GPU_DESCRIPTOR_HANDLE d3dDsvGPUDescriptorHandle = CScene::m_pDescriptorHeap->CreateShaderResourceView(
		m_pd3dDevice.Get(),
		m_pd3dDepthStencilBuffer.Get(),
		DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	);
	(void)d3dDsvGPUDescriptorHandle;

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

void CGameFramework::RequestSceneSwitch(ESceneId next)
{
	m_sceneSwitchPending = true;
	m_pendingScene = next;
}

void CGameFramework::ApplyPendingSceneSwitch()
{
	if (!m_sceneSwitchPending) return;

	const ESceneId next = m_pendingScene;
	m_sceneSwitchPending = false;
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

	if (scene)
	{
		CScene::ESceneRequest req;
		if (scene->ConsumeSceneRequest(req))
		{
			if (req == CScene::ESceneRequest::SwitchToGame)
				RequestSceneSwitch(ESceneId::Game);
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
	if (IsInputPauseActive())
		return;

	static UCHAR pKeysBuffer[256];
	bool bProcessedByScene = false;

	CScene* scene = m_SceneManager.GetScene();
	if (GetKeyboardState(pKeysBuffer) && scene)
		bProcessedByScene = scene->ProcessInput(pKeysBuffer);

	// Demo: 0/1/2/3 -> Player slot(0/1/2/3) Attack (edge trigger)
	static bool s_prevDown[4] = { false, false, false, false };
	for (int slot = 0; slot < 4; ++slot)
	{
		const bool down = (pKeysBuffer['0' + slot] & 0xF0) != 0;
		if (down && !s_prevDown[slot]) { if (scene) scene->RequestPlayerAttackBySlot(slot); }
		s_prevDown[slot] = down;
	}

	CGameObject* playerObj = (scene ? scene->GetPlayer() : nullptr);
	if (!playerObj) return;

	if (m_ptOldCursorPos.x == 0 && m_ptOldCursorPos.y == 0)
		::GetCursorPos(&m_ptOldCursorPos);

	auto* pc = playerObj->GetComponent<CPlayerControllerComponent>();
	if (!pc) return;

	DWORD dwDirection = 0;
	float cxDelta = 0.0f, cyDelta = 0.0f;
	bool bRunRequested = false;

	if (!bProcessedByScene)
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
		inputPkt.set_playerid(g_myPlayerId);
		inputPkt.set_keycodes(keyCodes);
#else
		if (pKeysBuffer['W'] & 0xF0) dwDirection |= DIR_FORWARD;
		if (pKeysBuffer['S'] & 0xF0) dwDirection |= DIR_BACKWARD;
		if (pKeysBuffer['A'] & 0xF0) dwDirection |= DIR_LEFT;
		if (pKeysBuffer['D'] & 0xF0) dwDirection |= DIR_RIGHT;
		if (pKeysBuffer[VK_PRIOR] & 0xF0) dwDirection |= DIR_UP;
		if (pKeysBuffer[VK_NEXT] & 0xF0)  dwDirection |= DIR_DOWN;

		bRunRequested =
			((pKeysBuffer[VK_LSHIFT] & 0xF0) != 0) ||
			((pKeysBuffer[VK_SHIFT] & 0xF0) != 0);

		static bool s_prevSpaceDown = false;
		const bool spaceDown = (pKeysBuffer[VK_SPACE] & 0xF0) != 0;

		if (spaceDown && !s_prevSpaceDown)
		{
			if (auto* animComp = playerObj->GetComponent<CAnimatorComponent>())
			{
				if (auto* ctrl = animComp->EnsureController())
					ctrl->RequestRoll(static_cast<uint32_t>(dwDirection));
			}
			else if (auto* ctrl = playerObj->GetAnimController())
			{
				ctrl->RequestRoll(static_cast<uint32_t>(dwDirection));
			}
		}

		s_prevSpaceDown = spaceDown;
#endif

		POINT ptCursorPos;
		if (GetCapture() == m_hWnd)
		{
			SetCursor(NULL);
			GetCursorPos(&ptCursorPos);
			cxDelta = (float)(ptCursorPos.x - m_ptOldCursorPos.x) / 3.0f;
			cyDelta = (float)(ptCursorPos.y - m_ptOldCursorPos.y) / 3.0f;
			SetCursorPos(m_ptOldCursorPos.x, m_ptOldCursorPos.y);
		}

		GetCursorPos(&ptCursorPos);

		cxDelta = (float)(ptCursorPos.x - m_ptOldCursorPos.x) / 3.0f;
		cyDelta = (float)(ptCursorPos.y - m_ptOldCursorPos.y) / 3.0f;

		m_ptOldCursorPos = ptCursorPos;

#ifdef USING_NETWORK
		inputPkt.set_deltax(cxDelta);
		inputPkt.set_deltay(cyDelta);

		auto sendBuffer = ServerPacketHandler::MakeSendBuffer(inputPkt);
		g_clientService->BroadCast(sendBuffer);
#else
		if (cxDelta || cyDelta)
		{
			if (pKeysBuffer[VK_RBUTTON] & 0xF0)
				pc->Rotate(cyDelta, 0.0f, -cxDelta);
			else
				pc->Rotate(cyDelta, cxDelta, 0.0f);
		}
#endif
	}

#ifdef USING_NETWORK
	// 서버로 부터 받은 현재 좌표를 기반으로 카메라 회전



#else
	const float dt = m_GameTimer.GetTimeElapsed();

	XMFLOAT3 oldPos = playerObj->GetPosition();

	if (cxDelta || cyDelta)
	{
		if (pKeysBuffer[VK_RBUTTON] & 0xF0)
			pc->Rotate(cyDelta, 0.0f, -cxDelta);
		else
			pc->Rotate(cyDelta, cxDelta, 0.0f);
	}

	pc->SetRunRequested(bRunRequested);

	if (dwDirection)
		pc->Move(dwDirection, 5.0f * dt, false);

	pc->SetInputDirection(static_cast<uint32_t>(dwDirection));

	XMFLOAT3 newPos = playerObj->GetPosition();

	if (m_pCamera)
	{
		XMFLOAT3 delta = Vector3::Subtract(newPos, oldPos);
		m_pCamera->Move(delta);

		m_pCamera->Update(newPos, dt);
		m_pCamera->SetLookAt(newPos);
		m_pCamera->RegenerateViewMatrix();
	}
#endif
	
}

void CGameFramework::AnimateObjects()
{
	CScene* scene = m_SceneManager.GetScene();
	if (scene) scene->AnimateObjects(m_GameTimer.GetTimeElapsed());
}

void CGameFramework::CollisionSystem()
{
	CScene* scene = m_SceneManager.GetScene();
	if (scene) scene->CollisionObjects();
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
	HRESULT hResult;

	m_GameTimer.Tick(0.0f);
	UpdateWindowActivationState();

	ApplyPendingSceneSwitch();

	ProcessInput();
	AnimateObjects();
	CollisionSystem();
	// 충돌체크 함수 필요

	hResult = m_pd3dCommandAllocator->Reset();
	hResult = m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), nullptr);

	::SynchronizeResourceTransition(
		m_pd3dCommandList.Get(),
		m_ppd3dSwapChainBackBuffers[m_nSwapChainBufferIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	CScene* scene = m_SceneManager.GetScene();
	if (scene)
		scene->OnPrepareRender(m_pd3dCommandList.Get(), m_pCamera);

	if (m_nDrawOption == DRAW_SCENE_COLOR)
	{
		m_pd3dCommandList->ClearDepthStencilView(
			m_d3dDsvDescriptorCPUHandle,
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			1.0f,
			0,
			0,
			nullptr
		);

		m_pPostProcessingShader->OnPrepareRenderTarget(
			m_pd3dCommandList.Get(),
			1,
			&m_pd3dSwapChainBackBufferRTVCPUHandles[m_nSwapChainBufferIndex],
			&m_d3dDsvDescriptorCPUHandle
		);

		if (scene)
			scene->Render(m_pd3dCommandList.Get(), m_pCamera);

		m_pPostProcessingShader->OnPostRenderTarget(m_pd3dCommandList.Get());
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

	ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
	m_pd3dCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
	WaitForGpuComplete();

	m_pdxgiSwapChain->Present(0, 0);

	MoveToNextFrame();

	m_GameTimer.GetFrameRate(m_pszFrameRate + 12, 37);
	::SetWindowText(m_hWnd, m_pszFrameRate);
}