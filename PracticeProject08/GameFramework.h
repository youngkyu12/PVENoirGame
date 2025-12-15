#pragma once
#include "Timer.h"
#include "Scene.h"
#include "Camera.h"

class CGameFramework {
public:
	CGameFramework();
	virtual ~CGameFramework();

	void WaitForGpuComplete();

// Build
public:
	bool OnCreate(HINSTANCE hInstance, HWND hMainWnd);

	void CreateDirect3DDevice();
	void InitDeviceCapabilities();
	void CreateFence();
	void CreateCommandQueueAndList();
	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateRenderTargetViews();
	void CreateDepthStencilView();

	void BuildObjects();

	void OnDestroy();

// Render
public:
	void ProcessInput();
	void AnimateObjects();
	void MoveToNextFrame();

	void FrameAdvance();

// Input
public:
	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM	lParam);
	void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void ChangeSwapChainState();
	void OnResizeBackBuffers();

// === Private Member Variables ===
private:
	HINSTANCE m_hInstance = nullptr;
	HWND m_hWnd = nullptr;
	int m_nWndClientWidth = FRAME_BUFFER_WIDTH;
	int m_nWndClientHeight = FRAME_BUFFER_HEIGHT;

	ComPtr<IDXGIFactory4> m_pdxgiFactory;
	ComPtr<IDXGISwapChain3> m_pdxgiSwapChain;
	ComPtr<ID3D12Device> m_pd3dDevice;

	UINT m_nMsaa4xQualityLevels = 0;

	static const UINT m_nSwapChainBuffers = 2;
	UINT m_nSwapChainBufferIndex = 0;

	vector<ComPtr<ID3D12Resource>> m_ppd3dSwapChainBackBuffers;
	ComPtr<ID3D12DescriptorHeap> m_pd3dRtvDescriptorHeap;
	UINT m_nRtvDescriptorIncrementSize = 0;

	ComPtr<ID3D12Resource> m_pd3dDepthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap> m_pd3dDsvDescriptorHeap;
	UINT m_nDsvDescriptorIncrementSize = 0;

	ComPtr<ID3D12CommandQueue> m_pd3dCommandQueue;
	ComPtr<ID3D12CommandAllocator> m_pd3dCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_pd3dCommandList;

	ComPtr<ID3D12PipelineState> m_pd3dPipelineState;

	ComPtr<ID3D12Fence> m_pd3dFence;
	UINT64 m_nFenceValues[m_nSwapChainBuffers]{ };
	HANDLE m_hFenceEvent = nullptr;

	unique_ptr<CScene> m_pScene;
	unique_ptr<CCamera> m_pCamera;

	CGameTimer m_GameTimer;

	_TCHAR m_pszFrameRate[50];
};