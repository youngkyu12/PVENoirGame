#pragma once

#define FRAME_BUFFER_WIDTH		640
#define FRAME_BUFFER_HEIGHT		480

#include "Timer.h"
#include "Player.h"
#include "Scene.h"

#define DRAW_SCENE_COLOR				'S'

#define DRAW_SCENE_TEXTURE				'T'
#define DRAW_SCENE_LIGHTING				'L'
#define DRAW_SCENE_NORMAL				'N'
#define DRAW_SCENE_Z_DEPTH				'Z'
#define DRAW_SCENE_DEPTH				'D'

class CGameFramework {
public:
	CGameFramework();
	~CGameFramework();

	void OnDestroy();
	void ReleaseObjects();

	void WaitForGpuComplete();

// Build
public:
	bool OnCreate(HINSTANCE hInstance, HWND hMainWnd);

	void CreateDirect3DDevice();
	void CreateCommandQueueAndList();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateSwapChain();
	void CreateSwapChainRenderTargetViews();
	void CreateDepthStencilView();
	void BuildObjects();

// Render
public:
	void ChangeSwapChainState();

    void ProcessInput();
    void AnimateObjects();
    void FrameAdvance();

	void MoveToNextFrame();

// Input
public:
	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

private:
	HINSTANCE							m_hInstance = nullptr;
	HWND								m_hWnd = nullptr;

	int									m_nWndClientWidth = FRAME_BUFFER_WIDTH;
	int									m_nWndClientHeight = FRAME_BUFFER_HEIGHT;
        
	ComPtr<IDXGIFactory4>				m_pdxgiFactory;
	ComPtr<IDXGISwapChain3>				m_pdxgiSwapChain;
	ComPtr<ID3D12Device>				m_pd3dDevice;

	bool								m_bMsaa4xEnable = false;
	UINT								m_nMsaa4xQualityLevels = 0;

	static const UINT					m_nSwapChainBuffers = 2;
	UINT								m_nSwapChainBufferIndex = 0;

	array<ComPtr<ID3D12Resource>, m_nSwapChainBuffers>			m_ppd3dSwapChainBackBuffers;
	ComPtr<ID3D12DescriptorHeap>								m_pd3dRtvDescriptorHeap;
	array<D3D12_CPU_DESCRIPTOR_HANDLE, m_nSwapChainBuffers>		m_pd3dSwapChainBackBufferRTVCPUHandles;

	ComPtr<ID3D12Resource>				m_pd3dDepthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap>		m_pd3dDsvDescriptorHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE			m_d3dDsvDescriptorCPUHandle;

	ComPtr<ID3D12CommandAllocator>		m_pd3dCommandAllocator;
	ComPtr<ID3D12CommandQueue>			m_pd3dCommandQueue;
	ComPtr<ID3D12GraphicsCommandList>	m_pd3dCommandList;

	ComPtr<ID3D12Fence>					m_pd3dFence;
	array<UINT64, m_nSwapChainBuffers>	m_nFenceValues;
	HANDLE								m_hFenceEvent = nullptr;

	CGameTimer							m_GameTimer;

	unique_ptr<CScene>					m_pScene;
	shared_ptr<CPlayer>					m_pPlayer;
	CCamera*							m_pCamera = nullptr;

	shared_ptr<CPostProcessingShader>	m_pPostProcessingShader;

	int								m_nDrawOption = DRAW_SCENE_COLOR;

	POINT							m_ptOldCursorPos;

	_TCHAR							m_pszFrameRate[50];
};

