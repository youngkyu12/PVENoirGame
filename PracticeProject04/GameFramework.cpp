#include "stdafx.h"
#include "GameFramework.h"

//------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------

GameFramework_Build::GameFramework_Build(shared_ptr<BuildRes> buildRes)
	: m_buildRes(buildRes)
{
	
}

GameFramework_Build::~GameFramework_Build()
{
}

bool GameFramework_Build::OnCreate(HINSTANCE hInstance, HWND hMainWnd)
{
	m_buildRes->m_hInstance = hInstance;
	m_buildRes->m_hWnd = hMainWnd;

	//Direct3D 디바이스, 명령 큐와 명령 리스트, 스왑 체인 등을 생성하는 함수를 호출한다. 
	CreateDirect3DDevice();
	CreateFence();
	CreateCommandQueueAndList();
	CreateRtvAndDsvDescriptorHeaps();
	CreateSwapChain();
	CreateDepthStencilView();

	BuildObjects();
	//렌더링할 게임 객체를 생성한다. 
	return (true);
}

void GameFramework_Build::OnDestroy()
{
	//GPU가 모든 명령 리스트를 실행할 때 까지 기다린다.
	//게임 객체(게임 월드 객체)를 소멸한다. 
	::CloseHandle(m_buildRes->m_hFenceEvent);

	m_buildRes->m_pdxgiSwapChain->SetFullscreenState(FALSE, NULL);

#if defined(_DEBUG)
	IDXGIDebug1* pdxgiDebug = NULL;
	DXGIGetDebugInterface1(0, __uuidof(IDXGIDebug1), (void**)&pdxgiDebug);
	HRESULT hResult = pdxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
	pdxgiDebug->Release();
#endif
}

void GameFramework_Build::CreateDirect3DDevice()
{
	HRESULT hResult;

	UINT nDXGIFactoryFlags = 0;

	ComPtr<IDXGIFactory4>& dxgiFactory = m_buildRes->m_pdxgiFactory;
#if defined(_DEBUG)
	ID3D12Debug* pd3dDebugController = NULL;
	hResult = D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&pd3dDebugController);
	if (pd3dDebugController) {
		pd3dDebugController->EnableDebugLayer();
		pd3dDebugController->Release();
	}
	nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	hResult = ::CreateDXGIFactory2(
		nDXGIFactoryFlags,
		IID_PPV_ARGS(dxgiFactory.GetAddressOf())
	);
	// DXGI 팩토리를 생성한다.

	ComPtr<IDXGIAdapter1> d3dAdapter;
	ComPtr<ID3D12Device>& d3dDevice = m_buildRes->m_pd3dDevice;

	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(i, &d3dAdapter); i++) {
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
		d3dAdapter->GetDesc1(&dxgiAdapterDesc);
		if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
		if (SUCCEEDED(D3D12CreateDevice(
			d3dAdapter.Get(),
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(d3dDevice.GetAddressOf())
		))) break;
	}
	//모든 하드웨어 어댑터 대하여 특성 레벨 12.0을 지원하는 하드웨어 디바이스를 생성한다.

	if (!d3dAdapter) {
		dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(d3dAdapter.GetAddressOf()));
		D3D12CreateDevice(d3dAdapter.Get(), D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(d3dDevice.GetAddressOf()));
	}
	//특성 레벨 12.0을 지원하는 하드웨어 디바이스를 생성할 수 없으면 WARP 디바이스를 생성한다.

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS d3dMsaaQualityLevels;
	d3dMsaaQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3dMsaaQualityLevels.SampleCount = 4; //Msaa4x 다중 샘플링
	d3dMsaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	d3dMsaaQualityLevels.NumQualityLevels = 0;

	d3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&d3dMsaaQualityLevels,
		sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS)
	);

	m_buildRes->m_nMsaa4xQualityLevels = d3dMsaaQualityLevels.NumQualityLevels;
	// 디바이스가 지원하는 다중 샘플의 품질 수준을 확인한다. 

	m_buildRes->m_bMsaa4xEnable = (m_buildRes->m_nMsaa4xQualityLevels > 1) ? true : false;
	// 다중 샘플의 품질 수준이 1보다 크면 다중 샘플링을 활성화한다. 

	
	m_buildRes->m_d3dViewport.TopLeftX = 0;
	m_buildRes->m_d3dViewport.TopLeftY = 0;
	m_buildRes->m_d3dViewport.Width = static_cast<float>(m_buildRes->m_nWndClientWidth);
	m_buildRes->m_d3dViewport.Height = static_cast<float>(m_buildRes->m_nWndClientHeight);
	m_buildRes->m_d3dViewport.MinDepth = 0.0f;
	m_buildRes->m_d3dViewport.MaxDepth = 1.0f;
	//뷰포트를 주 윈도우의 클라이언트 영역 전체로 설정한다.

	m_buildRes->m_d3dScissorRect = { 0, 0, m_buildRes->m_nWndClientWidth, m_buildRes->m_nWndClientHeight };
	// 씨저 사각형을 주 윈도우의 클라이언트 영역 전체로 설정한다.

}

void GameFramework_Build::CreateFence()
{
	HRESULT hResult;

	ComPtr<ID3D12Device>& d3dDevice = m_buildRes->m_pd3dDevice;
	ComPtr<ID3D12Fence>& d3dFence = m_buildRes->m_pd3dFence;

	hResult = d3dDevice->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(d3dFence.GetAddressOf())
	);
	//펜스를 생성하고 펜스 값을 0으로 설정한다.

	m_buildRes->m_hFenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	// 펜스와 동기화를 위한 이벤트 객체를 생성한다(이벤트 객체의 초기값을 FALSE이다). 
	// 이벤트가 실행되면(Signal) 이벤트의 값을 자동적으로 FALSE가 되도록 생성한다. 
}

void GameFramework_Build::CreateCommandQueueAndList()
{
	HRESULT hResult;

	ComPtr<ID3D12Device>& d3dDevice = m_buildRes->m_pd3dDevice;
	ComPtr<ID3D12CommandQueue>& d3dCommandQueue = m_buildRes->m_pd3dCommandQueue;
	ComPtr<ID3D12CommandAllocator>& d3dCommandAllocator = m_buildRes->m_pd3dCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList>& d3dCommandList = m_buildRes->m_pd3dCommandList;

	D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
	::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hResult = d3dDevice->CreateCommandQueue(
		&d3dCommandQueueDesc,
		IID_PPV_ARGS(d3dCommandQueue.GetAddressOf())
	);
	//직접(Direct) 명령 큐를 생성한다.
	
	hResult = d3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(d3dCommandAllocator.GetAddressOf())
	);
	//직접(Direct) 명령 할당자를 생성한다.

	hResult = d3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		d3dCommandAllocator.Get(),
		NULL,
		IID_PPV_ARGS(d3dCommandList.GetAddressOf())
	);
	//직접(Direct) 명령 리스트를 생성한다.

	hResult = d3dCommandList->Close();
	//명령 리스트는 생성되면 열린(Open) 상태이므로 닫힌(Closed) 상태로 만든다.
}

void GameFramework_Build::CreateSwapChain()
{
	ComPtr<IDXGIFactory4>& dxgiFactory = m_buildRes->m_pdxgiFactory;
	ComPtr<ID3D12CommandQueue>& d3dCommandQueue = m_buildRes->m_pd3dCommandQueue;
	ComPtr<IDXGISwapChain3>& dxgiSwapChain = m_buildRes->m_pdxgiSwapChain;

	RECT rcClient;
	::GetClientRect(m_buildRes->m_hWnd, &rcClient);
	m_buildRes->m_nWndClientWidth = rcClient.right - rcClient.left;
	m_buildRes->m_nWndClientHeight = rcClient.bottom - rcClient.top;

	DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc1;
	::ZeroMemory(&dxgiSwapChainDesc1, sizeof(DXGI_SWAP_CHAIN_DESC1));
	dxgiSwapChainDesc1.Width = m_buildRes->m_nWndClientWidth;
	dxgiSwapChainDesc1.Height = m_buildRes->m_nWndClientHeight;
	dxgiSwapChainDesc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc1.SampleDesc.Count = (m_buildRes->m_bMsaa4xEnable) ? 4 : 1;
	dxgiSwapChainDesc1.SampleDesc.Quality = (m_buildRes->m_bMsaa4xEnable) ? (m_buildRes->m_nMsaa4xQualityLevels - 1) : 0;
	dxgiSwapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dxgiSwapChainDesc1.BufferCount = m_buildRes->m_nSwapChainBuffers;
	dxgiSwapChainDesc1.Scaling = DXGI_SCALING_NONE;
	dxgiSwapChainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiSwapChainDesc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	dxgiSwapChainDesc1.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC dxgiSwapChainFullScreenDesc;
	::ZeroMemory(&dxgiSwapChainFullScreenDesc, sizeof(DXGI_SWAP_CHAIN_FULLSCREEN_DESC));
	dxgiSwapChainFullScreenDesc.RefreshRate.Numerator = 60;
	dxgiSwapChainFullScreenDesc.RefreshRate.Denominator = 1;
	dxgiSwapChainFullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	dxgiSwapChainFullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	dxgiSwapChainFullScreenDesc.Windowed = TRUE;

	dxgiFactory->CreateSwapChainForHwnd(
		d3dCommandQueue.Get(),
		m_buildRes->m_hWnd,
		&dxgiSwapChainDesc1,
		&dxgiSwapChainFullScreenDesc,
		NULL,
		(IDXGISwapChain1**)dxgiSwapChain.GetAddressOf()
	);
	//스왑체인을 생성한다. 

#ifndef _WITH_SWAPCHAIN_FULLSCREEN_STATE
	CreateRenderTargetViews();
#endif

	dxgiFactory->MakeWindowAssociation(m_buildRes->m_hWnd, DXGI_MWA_NO_ALT_ENTER);
	// "Alt+Enter" 키의 동작을 비활성화한다. 

	m_buildRes->m_nSwapChainBufferIndex = dxgiSwapChain->GetCurrentBackBufferIndex();
	//스왑체인의 현재 후면버퍼 인덱스를 저장한다. 


}

void GameFramework_Build::CreateRtvAndDsvDescriptorHeaps()
{
	ComPtr<ID3D12DescriptorHeap>& d3dRtvDescHeap = m_buildRes->m_pd3dRtvDescriptorHeap;
	ComPtr<ID3D12DescriptorHeap>& d3dDsvDescHeap = m_buildRes->m_pd3dDsvDescriptorHeap;
	ComPtr<ID3D12Device>& d3dDevice = m_buildRes->m_pd3dDevice;

	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
	::ZeroMemory(&d3dDescriptorHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	d3dDescriptorHeapDesc.NumDescriptors = m_buildRes->m_nSwapChainBuffers;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	d3dDescriptorHeapDesc.NodeMask = 0;

	HRESULT hResult = d3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(d3dRtvDescHeap.GetAddressOf())
	);
	//렌더 타겟 서술자 힙(서술자의 개수는 스왑체인 버퍼의 개수)을 생성한다.

	m_buildRes->m_nRtvDescriptorIncrementSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//렌더 타겟 서술자 힙의 원소의 크기를 저장한다.

	d3dDescriptorHeapDesc.NumDescriptors = 1;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	hResult = d3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(d3dDsvDescHeap.GetAddressOf())
	);
	//깊이-스텐실 서술자 힙(서술자의 개수는 1)을 생성한다.

	m_buildRes->m_nDsvDescriptorIncrementSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	//깊이-스텐실 서술자 힙의 원소의 크기를 저장한다.
}

void GameFramework_Build::BuildObjects()
{

}



void GameFramework_Build::CreateRenderTargetViews()
{
	ComPtr<IDXGISwapChain3>& dxgiSwapChain = m_buildRes->m_pdxgiSwapChain;
	ComPtr<ID3D12Device>& d3dDevice = m_buildRes->m_pd3dDevice;
	ComPtr<ID3D12DescriptorHeap>& d3dRtvDescHeap = m_buildRes->m_pd3dRtvDescriptorHeap;

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = d3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < m_buildRes->m_nSwapChainBuffers; i++)
	{
		dxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(m_buildRes->m_ppd3dSwapChainBackBuffers[i].GetAddressOf()));
		d3dDevice->CreateRenderTargetView(m_buildRes->m_ppd3dSwapChainBackBuffers[i].Get(), NULL, d3dRtvCPUDescriptorHandle);
		d3dRtvCPUDescriptorHandle.ptr += m_buildRes->m_nRtvDescriptorIncrementSize;
	}
}

void GameFramework_Build::CreateDepthStencilView()
{
	ComPtr<ID3D12Device>& d3dDevice = m_buildRes->m_pd3dDevice;
	ComPtr<ID3D12DescriptorHeap>& d3dDsvDescHeap = m_buildRes->m_pd3dDsvDescriptorHeap;
	ComPtr<ID3D12Resource>& d3dDepthStencilBuffer = m_buildRes->m_pd3dDepthStencilBuffer;

	D3D12_RESOURCE_DESC d3dResourceDesc;
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3dResourceDesc.Alignment = 0;
	d3dResourceDesc.Width = m_buildRes->m_nWndClientWidth;
	d3dResourceDesc.Height = m_buildRes->m_nWndClientHeight;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dResourceDesc.SampleDesc.Count = (m_buildRes->m_bMsaa4xEnable) ? 4 : 1;
	d3dResourceDesc.SampleDesc.Quality = (m_buildRes->m_bMsaa4xEnable) ? (m_buildRes->m_nMsaa4xQualityLevels - 1) : 0;
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

	d3dDevice->CreateCommittedResource(
		&d3dHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&d3dClearValue,
		IID_PPV_ARGS(d3dDepthStencilBuffer.GetAddressOf())
	);
	//깊이-스텐실 버퍼를 생성한다.

	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle = d3dDsvDescHeap->GetCPUDescriptorHandleForHeapStart();
	d3dDevice->CreateDepthStencilView(
		d3dDepthStencilBuffer.Get(),
		NULL,
		d3dDsvCPUDescriptorHandle
	);
	//깊이-스텐실 버퍼 뷰를 생성한다.
}

//------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------

GameFramework_Render::GameFramework_Render(shared_ptr<BuildRes> buildRes)
	: m_buildRes(buildRes)
{
	_tcscpy_s(m_pszFrameRate, _T("LapProject ("));
}

GameFramework_Render::~GameFramework_Render()
{
}

void GameFramework_Render::OnDestroy()
{
	ReleaseObjects();
}

void GameFramework_Render::ReleaseObjects()
{
}

void GameFramework_Render::CreateRenderTargetViews()
{
	ComPtr<IDXGISwapChain3>& dxgiSwapChain = m_buildRes->m_pdxgiSwapChain;
	ComPtr<ID3D12Device>& d3dDevice = m_buildRes->m_pd3dDevice;
	ComPtr<ID3D12DescriptorHeap>& d3dRtvDescHeap = m_buildRes->m_pd3dRtvDescriptorHeap;

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = d3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < m_buildRes->m_nSwapChainBuffers; i++)
	{
		dxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(m_buildRes->m_ppd3dSwapChainBackBuffers[i].GetAddressOf()));
		d3dDevice->CreateRenderTargetView(m_buildRes->m_ppd3dSwapChainBackBuffers[i].Get(), NULL, d3dRtvCPUDescriptorHandle);
		d3dRtvCPUDescriptorHandle.ptr += m_buildRes->m_nRtvDescriptorIncrementSize;
	}
}

void GameFramework_Render::CreateDepthStencilView()
{
	ComPtr<ID3D12Device>& d3dDevice = m_buildRes->m_pd3dDevice;
	ComPtr<ID3D12DescriptorHeap>& d3dDsvDescHeap = m_buildRes->m_pd3dDsvDescriptorHeap;
	ComPtr<ID3D12Resource>& d3dDepthStencilBuffer = m_buildRes->m_pd3dDepthStencilBuffer;

	D3D12_RESOURCE_DESC d3dResourceDesc;
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3dResourceDesc.Alignment = 0;
	d3dResourceDesc.Width = m_buildRes->m_nWndClientWidth;
	d3dResourceDesc.Height = m_buildRes->m_nWndClientHeight;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dResourceDesc.SampleDesc.Count = (m_buildRes->m_bMsaa4xEnable) ? 4 : 1;
	d3dResourceDesc.SampleDesc.Quality = (m_buildRes->m_bMsaa4xEnable) ? (m_buildRes->m_nMsaa4xQualityLevels - 1) : 0;
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

	d3dDevice->CreateCommittedResource(
		&d3dHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&d3dClearValue,
		IID_PPV_ARGS(d3dDepthStencilBuffer.GetAddressOf())
	);
	//깊이-스텐실 버퍼를 생성한다.

	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle = d3dDsvDescHeap->GetCPUDescriptorHandleForHeapStart();
	d3dDevice->CreateDepthStencilView(
		d3dDepthStencilBuffer.Get(),
		NULL,
		d3dDsvCPUDescriptorHandle
	);
	//깊이-스텐실 버퍼 뷰를 생성한다.
}

void GameFramework_Render::ChangeSwapChainState()
{
	ComPtr<IDXGISwapChain3>& dxgiSwapChain = m_buildRes->m_pdxgiSwapChain;

	BOOL bFullScreenState = FALSE;
	dxgiSwapChain->GetFullscreenState(&bFullScreenState, NULL);
	dxgiSwapChain->SetFullscreenState(!bFullScreenState, NULL);
	// 전체화면 윈도우 모드 전환

	DXGI_MODE_DESC dxgiTargetParameters;
	dxgiTargetParameters.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiTargetParameters.Width = m_buildRes->m_nWndClientWidth;
	dxgiTargetParameters.Height = m_buildRes->m_nWndClientHeight;
	dxgiTargetParameters.RefreshRate.Numerator = 60;
	dxgiTargetParameters.RefreshRate.Denominator = 1;
	dxgiTargetParameters.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	dxgiTargetParameters.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	dxgiSwapChain->ResizeTarget(&dxgiTargetParameters);
	// 출력할 크기를 응용프로그램 크기로 정함

	OnResizeBackBuffers();
}

void GameFramework_Render::OnResizeBackBuffers()
{
	ComPtr<ID3D12CommandAllocator>& d3dCommandAllocator = m_buildRes->m_pd3dCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList>& d3dCommandList = m_buildRes->m_pd3dCommandList;
	ComPtr<IDXGIFactory4>& dxgiFactory = m_buildRes->m_pdxgiFactory;
	ComPtr<IDXGISwapChain3>& dxgiSwapChain = m_buildRes->m_pdxgiSwapChain;

	WaitForGpuComplete();
	for (int i = 0; i < m_buildRes->m_nSwapChainBuffers; i++)
		if (m_buildRes->m_ppd3dSwapChainBackBuffers[i])
			m_buildRes->m_ppd3dSwapChainBackBuffers[i]->Release();
	// 기존 리소스 해제

	DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc1;
	dxgiSwapChain->GetDesc1(&dxgiSwapChainDesc1);
	dxgiSwapChain->ResizeBuffers(
		m_buildRes->m_nSwapChainBuffers,
		m_buildRes->m_nWndClientWidth,
		m_buildRes->m_nWndClientHeight,
		dxgiSwapChainDesc1.Format,
		dxgiSwapChainDesc1.Flags
	);

	m_buildRes->m_nSwapChainBufferIndex = dxgiSwapChain->GetCurrentBackBufferIndex();

	CreateRenderTargetViews();
}

void GameFramework_Render::ProcessInput()
{
}

void GameFramework_Render::AnimateObjects()
{
	
}

void GameFramework_Render::FrameAdvance()
{
	ComPtr<ID3D12CommandQueue>& d3dCommandQueue = m_buildRes->m_pd3dCommandQueue;
	ComPtr<ID3D12CommandAllocator>& d3dCommandAllocator = m_buildRes->m_pd3dCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList>& d3dCommandList = m_buildRes->m_pd3dCommandList;
	ComPtr<IDXGIFactory4>& dxgiFactory = m_buildRes->m_pdxgiFactory;
	ComPtr<IDXGISwapChain3>& dxgiSwapChain = m_buildRes->m_pdxgiSwapChain;
	ComPtr<ID3D12Resource>& d3dDepthStencilBuffer = m_buildRes->m_pd3dDepthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap>& d3dRtvDescHeap = m_buildRes->m_pd3dRtvDescriptorHeap;
	ComPtr<ID3D12DescriptorHeap>& d3dDsvDescHeap = m_buildRes->m_pd3dDsvDescriptorHeap;

	// 타이머의 시간이 갱신되도록 하고 프레임 레이트를 계산한다. 
	m_buildRes->m_GameTimer.Tick(0.0f);

	ProcessInput();

	AnimateObjects();

	HRESULT hResult = d3dCommandAllocator->Reset();
	hResult = d3dCommandList->Reset(d3dCommandAllocator.Get(), NULL);
	// 명령 할당자와 명령 리스트를 리셋한다.

	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource = m_buildRes->m_ppd3dSwapChainBackBuffers[m_buildRes->m_nSwapChainBufferIndex].Get();
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	d3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);
	// 현재 렌더 타겟에 대한 프리젠트가 끝나기를 기다린다. 
	// 프리젠트가 끝나면 렌더 타겟 버퍼의 상태는 프리젠트 상태(D3D12_RESOURCE_STATE_PRESENT)에서 
	// 렌더 타겟 상태(D3D12_RESOURCE_STATE_RENDER_TARGET)로 바뀔 것이다.

	d3dCommandList->RSSetViewports(1, &m_buildRes->m_d3dViewport);
	d3dCommandList->RSSetScissorRects(1, &m_buildRes->m_d3dScissorRect);
	// 뷰포트와 씨저 사각형을 설정한다.

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = d3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (m_buildRes->m_nSwapChainBufferIndex * m_buildRes->m_nRtvDescriptorIncrementSize);
	// 현재의 렌더 타겟에 해당하는 서술자의 CPU 주소(핸들)를 계산한다.

	float pfClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
	d3dCommandList->ClearRenderTargetView(
		d3dRtvCPUDescriptorHandle,
		pfClearColor/*Colors::Azure*/,
		0,
		NULL
	);
	// 원하는 색상으로 렌더 타겟(뷰)을 지운다.

	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle = d3dDsvDescHeap->GetCPUDescriptorHandleForHeapStart();
	// 깊이-스텐실 서술자의 CPU 주소를 계산한다.

	d3dCommandList->ClearDepthStencilView(
		d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f,
		0,
		0,
		NULL
	);
	// 원하는 값으로 깊이-스텐실(뷰)을 지운다.

	d3dCommandList->OMSetRenderTargets(
		1,
		&d3dRtvCPUDescriptorHandle,
		TRUE,
		&d3dDsvCPUDescriptorHandle
	);
	// 렌더 타겟 뷰(서술자)와 깊이-스텐실 뷰(서술자)를 출력-병합 단계(OM)에 연결한다.

	// 렌더링 코드는 여기에 추가될 것이다.

	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	d3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);
	// 현재 렌더 타겟에 대한 렌더링이 끝나기를 기다린다. 
	// GPU가 렌더 타겟(버퍼)을 더 이상 사용하지 않으면 렌더 타겟의 상태는 
	// 프리젠트 상태(D3D12_RESOURCE_STATE_PRESENT)로 바뀔 것이다.

	hResult = d3dCommandList->Close();
	// 명령 리스트를 닫힌 상태로 만든다.

	ID3D12CommandList* ppd3dCommandLists[] = { d3dCommandList.Get()};
	d3dCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
	// 명령 리스트를 명령 큐에 추가하여 실행한다.

	WaitForGpuComplete();
	// GPU가 모든 명령 리스트를 실행할 때 까지 기다린다.

	DXGI_PRESENT_PARAMETERS dxgiPresentParameters;
	dxgiPresentParameters.DirtyRectsCount = 0;
	dxgiPresentParameters.pDirtyRects = NULL;
	dxgiPresentParameters.pScrollRect = NULL;
	dxgiPresentParameters.pScrollOffset = NULL;
	dxgiSwapChain->Present1(1, 0, &dxgiPresentParameters);
	// 스왑체인을 프리젠트한다. 
	// 프리젠트를 하면 현재 렌더 타겟(후면버퍼)의 내용이 전면버퍼로 옮겨지고 렌더 타겟 인덱스가 바뀔 것이다.

	m_buildRes->m_nSwapChainBufferIndex = dxgiSwapChain->GetCurrentBackBufferIndex();

	// 현재의 프레임 레이트를 문자열로 가져와서 주 윈도우의 타이틀로 출력한다. 
	// m_pszBuffer 문자열이"LapProject ("으로 초기화되었으므로 (m_pszFrameRate+12)에서부터 프레임 레이트를 문자열로 출력하여 "FPS)" 문자열과 합친다.
	// ::_itow_s(m_nCurrentFrameRate, (m_pszFrameRate+12), 37, 10);
	// ::wcscat_s((m_pszFrameRate+12), 37, _T(" FPS)"));
	m_buildRes->m_GameTimer.GetFrameRate(m_pszFrameRate + 12, 37);
	::SetWindowText(m_buildRes->m_hWnd, m_pszFrameRate);
}

void GameFramework_Render::MoveToNextFrame()
{
	ComPtr<ID3D12CommandQueue>& d3dCommandQueue = m_buildRes->m_pd3dCommandQueue;
	ComPtr<IDXGISwapChain3>& dxgiSwapChain = m_buildRes->m_pdxgiSwapChain;
	ComPtr<ID3D12Fence>& d3dFence = m_buildRes->m_pd3dFence;

	m_buildRes->m_nSwapChainBufferIndex = dxgiSwapChain->GetCurrentBackBufferIndex();
	UINT64 nFenceValue = ++m_buildRes->m_nFenceValue;
	HRESULT hResult = d3dCommandQueue->Signal(d3dFence.Get(), nFenceValue);
	if (d3dFence->GetCompletedValue() < nFenceValue) {
		hResult = d3dFence->SetEventOnCompletion(nFenceValue, m_buildRes->m_hFenceEvent);
		::WaitForSingleObject(m_buildRes->m_hFenceEvent, INFINITE);
	}
}

void GameFramework_Render::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
		break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		break;
	case WM_MOUSEMOVE:
		break;
	default:
		break;
	}
}

void GameFramework_Render::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_KEYUP:
		switch (wParam)
		{
		case VK_ESCAPE:
			::PostQuitMessage(0);
			break;
		case VK_RETURN:
			break;
		case VK_F8:
			break;
		case VK_F9:
			ChangeSwapChainState();
		default:
			break;
		}
		break;
	default:
		break;
	}
}

LRESULT GameFramework_Render::OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_SIZE:
	{
		m_buildRes->m_nWndClientWidth = LOWORD(lParam);
		m_buildRes->m_nWndClientHeight = HIWORD(lParam);
		break;
	}
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

void GameFramework_Render::WaitForGpuComplete()
{
	ComPtr<ID3D12CommandQueue>& d3dCommandQueue = m_buildRes->m_pd3dCommandQueue;
	ComPtr<ID3D12Fence>& d3dFence = m_buildRes->m_pd3dFence;

	m_buildRes->m_nFenceValue++;
	//CPU 펜스의 값을 증가한다.

	const UINT64 nFence = m_buildRes->m_nFenceValue;
	HRESULT hResult = d3dCommandQueue->Signal(d3dFence.Get(), nFence);
	//GPU가 펜스의 값을 설정하는 명령을 명령 큐에 추가한다.

	if (d3dFence->GetCompletedValue() < nFence) {
		//펜스의 현재 값이 설정한 값보다 작으면 펜스의 현재 값이 설정한 값이 될 때까지 기다린다.
		hResult = d3dFence->SetEventOnCompletion(nFence, m_buildRes->m_hFenceEvent);
		::WaitForSingleObject(m_buildRes->m_hFenceEvent, INFINITE);
	}
}