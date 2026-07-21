//-----------------------------------------------------------------------------
// File: CGameFramework.h
//-----------------------------------------------------------------------------
#pragma once

#include "Timer.h"
#include "SceneManager.h"

#define DRAW_SCENE_COLOR				'S'

#define DRAW_SCENE_TEXTURE				'T'
#define DRAW_SCENE_LIGHTING				'L'
#define DRAW_SCENE_NORMAL				'N'
#define DRAW_SCENE_Z_DEPTH				'Z'
#define DRAW_SCENE_DEPTH				'D'

enum class DisplayMode{
	Windowed,
	BorderlessFullscreen,
	ExclusiveFullscreen
};

class CScene;
class CCamera;
class CPostProcessingShader;
class CAudioManager;

class CGameFramework {
public:
	CGameFramework();
	~CGameFramework();

	void SetDisplayMode(DisplayMode DM, int Width, int Height);
	
	// Lifecycle
public:
	void OnDestroy();
	void ReleaseObjects();
	UINT64 SignalCommandQueue();
	void WaitForFenceValue(UINT64 fenceValue);
	void WaitForFrameContext(UINT frameContextIndex);
	void FlushGpu();

	// Build
public:
	bool OnCreate(HINSTANCE hInstance, HWND hMainWnd);

	void CreateDirect3DDevice();
	void FindOutputForCurrentWindow();
	void CreateCommandQueueAndList();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateSwapChain();
	void CreateSwapChainRenderTargetViews();
	void CreateDepthStencilView();

	void BuildObjects();

	// Frame / Render
public:
	void ChangeSwapChainState();
	void OnResize(int width, int height);
	void EnterBorderlessFullscreen();
	void LeaveBorderlessFullscreen();
	bool CanUseExclusiveFullscreen() const;

	void ProcessInput();
	void AnimateObjects();
	void CollisionSystem();
	void FrameAdvance();

	void MoveToNextFrame();

	// Window / Input
public:
	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

private:
	// Scene switching (Menu -> Game)
	void RequestSceneSwitch(ESceneId next, bool presentCurrentSceneOnceBeforeSwitch = false);
	void ApplyPendingSceneSwitch();
	void BuildSceneInternal(ESceneId id, bool resetTimer);
	void SyncGameSceneInactiveOverlay();
	bool IsWindowActuallyActive() const;
	void UpdateWindowActivationState();
	bool IsInputPauseActive() const;
	void UpdateAudioListener();

	bool IsGameSceneActive() const;
	bool ShouldLockGameCursor() const;
	POINT GetClientCenterScreenPoint() const;
	void LockGameCursor();
	void UnlockGameCursor();
	void UpdateGameCursorLock();

	

	bool								m_sceneSwitchPending = false;
	ESceneId							m_pendingScene = ESceneId::Menu;
	bool								m_sceneSwitchReadyToApply = false;
	bool								m_bResizePending = false;

private:
	// Window
	HINSTANCE							m_hInstance = nullptr;
	HWND								m_hWnd = nullptr;

	// DXGI / Device
	ComPtr<IDXGIFactory6>				m_pdxgiFactory;
	ComPtr<IDXGIAdapter1>				m_pd3dGPUAdapter;
	ComPtr<IDXGISwapChain3>				m_pdxgiSwapChain;
	ComPtr<ID3D12Device>				m_pd3dDevice;

	bool								m_bMsaa4xEnable = false;
	UINT								m_nMsaa4xQualityLevels = 0;

	// SwapChain Buffers / RTV
	static const UINT					m_nSwapChainBuffers = 2;
	static const UINT					m_nFrameContexts = 2;
	UINT								m_nSwapChainBufferIndex = 0;

	array<ComPtr<ID3D12Resource>, m_nSwapChainBuffers>			m_ppd3dSwapChainBackBuffers;
	ComPtr<ID3D12DescriptorHeap>								m_pd3dRtvDescriptorHeap;
	array<D3D12_CPU_DESCRIPTOR_HANDLE, m_nSwapChainBuffers>		m_pd3dSwapChainBackBufferRTVCPUHandles;

	// DSV
	ComPtr<ID3D12Resource>				m_pd3dDepthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap>		m_pd3dDsvDescriptorHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE			m_d3dDsvDescriptorCPUHandle;

	// Command
	struct FrameContext
	{
		ComPtr<ID3D12CommandAllocator>		commandAllocator;
		ComPtr<ID3D12GraphicsCommandList>	commandList;
		UINT64								fenceValue = 0;
	};

	array<FrameContext, m_nFrameContexts>	m_frameContexts;
	UINT									m_nFrameContextIndex = 0;

	// 현재 frame context를 기존 코드가 그대로 쓰도록 유지하는 호환용 alias.
	ComPtr<ID3D12CommandAllocator>			m_pd3dCommandAllocator;
	ComPtr<ID3D12CommandQueue>				m_pd3dCommandQueue;
	ComPtr<ID3D12GraphicsCommandList>		m_pd3dCommandList;
	ComPtr<ID3D12InfoQueue>					infoQueue;

	// Sync
	ComPtr<ID3D12Fence>					m_pd3dFence;
	UINT64								m_nNextFenceValue = 1;
	HANDLE								m_hFenceEvent = nullptr;

	// Timer
	CGameTimer							m_GameTimer;

	// Scene Manager / Camera
	CSceneManager						m_SceneManager;
	CCamera*							m_pCamera = nullptr;

	// Post Processing
	shared_ptr<CPostProcessingShader>	m_pPostProcessingShader;
	std::unique_ptr<CAudioManager>		m_pAudioManager;

	// Render Option
	int									m_nDrawOption = DRAW_SCENE_COLOR;

	// Input State
	POINT								m_ptOldCursorPos;

	// UI Text
	_TCHAR								m_pszFrameRate[50];
	
	bool								m_bWindowActive = true;
	bool								m_bConsumeNextMouseClick = false;
	bool								m_bUserPaused = false;
	bool								m_bGameCursorLocked = false;

	// AdapterDisplayModes
	vector<DXGI_MODE_DESC>				m_DisplayModeList;
	DXGI_OUTPUT_DESC					m_OutputDesc {};

	int									m_nWndClientWidth = FRAME_BUFFER_WIDTH;
	int									m_nWndClientHeight = FRAME_BUFFER_HEIGHT;

	DisplayMode							m_DisplayMode = DisplayMode::Windowed;
	bool								m_bHasGpuOutput = false;
	DWORD								m_dwWindowedStyle = 0;
	WINDOWPLACEMENT						m_WindowedPlacement {};
	bool								m_Fullscreen = false;

	bool HandlePauseClick(UINT nMessageID, LPARAM lParam);
	void ClearInputPause();
};