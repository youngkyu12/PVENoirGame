#include "stdafx.h"
#include "GameFramework.h"

CGameFramework::CGameFramework()
{
	m_hFenceEvent = NULL;
	for (int i = 0; i < m_nSwapChainBuffers; i++) m_nFenceValues[i] = 0;

	_tcscpy_s(m_pszFrameRate, _T("LapProject ("));
}

CGameFramework::~CGameFramework()
{
}


void CGameFramework::OnDestroy()
{
	WaitForGpuComplete();
	//GPU가 모든 명령 리스트를 실행할 때 까지 기다린다.
	//게임 객체(게임 월드 객체)를 소멸한다. 
	::CloseHandle(m_hFenceEvent);
	m_pdxgiSwapChain->SetFullscreenState(FALSE, NULL);


#if defined(_DEBUG)
	IDXGIDebug1* pdxgiDebug = NULL;
	DXGIGetDebugInterface1(0, __uuidof(IDXGIDebug1), (void**)&pdxgiDebug);
	HRESULT hResult = pdxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL,	DXGI_DEBUG_RLO_DETAIL);
	pdxgiDebug->Release();
#endif
}

void CGameFramework::WaitForGpuComplete()
{
	UINT64 nFenceValue = ++m_nFenceValues[m_nSwapChainBufferIndex];
	//CPU 펜스의 값을 증가한다.

	HRESULT hResult = m_pd3dCommandQueue->Signal(m_pd3dFence.Get(), nFenceValue);
	//GPU가 펜스의 값을 설정하는 명령을 명령 큐에 추가한다.

	if (m_pd3dFence->GetCompletedValue() < nFenceValue) {
		//펜스의 현재 값이 설정한 값보다 작으면 펜스의 현재 값이 설정한 값이 될 때까지 기다린다.
		hResult = m_pd3dFence->SetEventOnCompletion(nFenceValue, m_hFenceEvent);
		::WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}

