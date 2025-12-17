#include "stdafx.h"
#include "GameFramework.h"

CGameFramework::CGameFramework()
{
	_tcscpy_s(m_pszFrameRate, _T("LapProject ("));
}

CGameFramework::~CGameFramework()
{
}

void CGameFramework::OnDestroy()
{
	WaitForGpuComplete();

	ReleaseObjects();

	::CloseHandle(m_hFenceEvent);
	for (int i = 0; i < m_nSwapChainBuffers; i++)
		if (m_ppd3dSwapChainBackBuffers[i])
			m_ppd3dSwapChainBackBuffers[i].Reset();
	if (m_pd3dRtvDescriptorHeap) m_pd3dRtvDescriptorHeap.Reset();
	if (m_pd3dDepthStencilBuffer) m_pd3dDepthStencilBuffer.Reset();
	if (m_pd3dDsvDescriptorHeap) m_pd3dDsvDescriptorHeap.Reset();
	if (m_pd3dCommandAllocator) m_pd3dCommandAllocator.Reset();
	if (m_pd3dCommandQueue) m_pd3dCommandQueue.Reset();
	if (m_pd3dPipelineState) m_pd3dPipelineState.Reset();
	if (m_pd3dCommandList) m_pd3dCommandList.Reset();
	if (m_pd3dFence) m_pd3dFence.Reset();

	m_pdxgiSwapChain->SetFullscreenState(FALSE, NULL);
	if (m_pdxgiSwapChain) m_pdxgiSwapChain.Reset();
	if (m_pd3dDevice) m_pd3dDevice.Reset();
	if (m_pdxgiFactory) m_pdxgiFactory.Reset();


#if defined(_DEBUG)
	ComPtr<IDXGIDebug1> pdxgiDebug;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pdxgiDebug));
	HRESULT hResult = pdxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
#endif
}

void CGameFramework::ReleaseObjects()
{
	if (m_pScene) m_pScene->ReleaseObjects();
	if (m_pPlayer) m_pPlayer->ReleaseObjects();
}


void CGameFramework::WaitForGpuComplete()
{
	UINT64 nFenceValue = ++m_nFenceValues[m_nSwapChainBufferIndex];

	HRESULT hResult = m_pd3dCommandQueue->Signal(m_pd3dFence.Get(), nFenceValue);

	if (m_pd3dFence->GetCompletedValue() < nFenceValue) {
		hResult = m_pd3dFence->SetEventOnCompletion(nFenceValue, m_hFenceEvent);
		::WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}
