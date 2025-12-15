#pragma once
#include "Timer.h"

#include <d3d12.h>
#include <dxgi1_4.h>

using Microsoft::WRL::ComPtr;

struct BuildRes {
	HINSTANCE m_hInstance;
	HWND m_hWnd;

	int m_nWndClientWidth;
	int m_nWndClientHeight;

	ComPtr<IDXGIFactory4> m_pdxgiFactory;
	ComPtr<IDXGISwapChain3> m_pdxgiSwapChain;
	ComPtr<ID3D12Device> m_pd3dDevice;

	static const UINT m_nSwapChainBuffers = 2;
	UINT m_nSwapChainBufferIndex = 0;

	ComPtr<ID3D12Resource> m_ppd3dSwapChainBackBuffers[m_nSwapChainBuffers];
	ComPtr<ID3D12DescriptorHeap> m_pd3dRtvDescriptorHeap;
	UINT m_nRtvDescriptorIncrementSize = 0;

	ComPtr<ID3D12Resource> m_pd3dDepthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap> m_pd3dDsvDescriptorHeap;
	UINT m_nDsvDescriptorIncrementSize = 0;

	ComPtr<ID3D12CommandQueue> m_pd3dCommandQueue;
	ComPtr<ID3D12CommandAllocator> m_pd3dCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_pd3dCommandList;

	ComPtr<ID3D12Fence> m_pd3dFence;
	UINT64 m_nFenceValue;
	HANDLE m_hFenceEvent;

	D3D12_VIEWPORT m_d3dViewport;
	D3D12_RECT m_d3dScissorRect;

	CGameTimer m_GameTimer;

	bool m_bMsaa4xEnable = false;
	UINT m_nMsaa4xQualityLevels = 0;
};