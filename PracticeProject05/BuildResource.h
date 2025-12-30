#pragma once
#include "Timer.h"
#include "stdafx.h"

#include <d3d12.h>
#include <dxgi1_4.h>

using Microsoft::WRL::ComPtr;

struct BuildRes {
	HINSTANCE m_hInstance = nullptr;
	HWND m_hWnd = nullptr;

	int m_nWndClientWidth = FRAME_BUFFER_WIDTH;;
	int m_nWndClientHeight = FRAME_BUFFER_HEIGHT;;

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

	ComPtr<ID3D12RootSignature> m_pd3dGraphicsRootSignature;
	ComPtr <ID3D12PipelineState> m_pd3dPipelineState;

	ComPtr<ID3D12Fence> m_pd3dFence;
	UINT64 m_nFenceValues[m_nSwapChainBuffers]{};
	HANDLE m_hFenceEvent = nullptr;

	D3D12_VIEWPORT m_d3dViewport = { 0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f };
	D3D12_RECT m_d3dScissorRect = { 0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT };

	CGameTimer m_GameTimer{};

	bool m_bMsaa4xEnable = false;
	UINT m_nMsaa4xQualityLevels = 0;
};