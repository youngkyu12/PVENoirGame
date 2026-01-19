//-----------------------------------------------------------------------------
// File: CGameFramework_Render.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameFramework.h"

void CGameFramework::ProcessInput()
{
	static UCHAR pKeysBuffer[256];
	bool bProcessedByScene = false;

	if (GetKeyboardState(pKeysBuffer) && m_pScene)
		bProcessedByScene = m_pScene->ProcessInput(pKeysBuffer);

	if (!bProcessedByScene)
	{
		DWORD dwDirection = 0;
		if (pKeysBuffer[VK_UP] & 0xF0)
			dwDirection |= DIR_FORWARD;

		if (pKeysBuffer[VK_DOWN] & 0xF0)
			dwDirection |= DIR_BACKWARD;

		if (pKeysBuffer[VK_LEFT] & 0xF0)
			dwDirection |= DIR_LEFT;

		if (pKeysBuffer[VK_RIGHT] & 0xF0)
			dwDirection |= DIR_RIGHT;

		if (pKeysBuffer[VK_PRIOR] & 0xF0)
			dwDirection |= DIR_UP;

		if (pKeysBuffer[VK_NEXT] & 0xF0)
			dwDirection |= DIR_DOWN;

		float cxDelta = 0.0f, cyDelta = 0.0f;
		POINT ptCursorPos;

		if (GetCapture() == m_hWnd)
		{
			SetCursor(NULL);
			GetCursorPos(&ptCursorPos);
			cxDelta = (float)(ptCursorPos.x - m_ptOldCursorPos.x) / 3.0f;
			cyDelta = (float)(ptCursorPos.y - m_ptOldCursorPos.y) / 3.0f;
			SetCursorPos(m_ptOldCursorPos.x, m_ptOldCursorPos.y);
		}

		if ((dwDirection != 0) || (cxDelta != 0.0f) || (cyDelta != 0.0f))
		{
			if (cxDelta || cyDelta)
			{
				if (pKeysBuffer[VK_RBUTTON] & 0xF0)
					m_pPlayer->Rotate(cyDelta, 0.0f, -cxDelta);
				else
					m_pPlayer->Rotate(cyDelta, cxDelta, 0.0f);
			}
			if (dwDirection)m_pPlayer->Move(dwDirection, 50.0f * m_GameTimer.GetTimeElapsed(), true);
		}
	}
	m_pPlayer->Update(m_GameTimer.GetTimeElapsed());
}

void CGameFramework::AnimateObjects()
{
	if (m_pScene)m_pScene->AnimateObjects(m_GameTimer.GetTimeElapsed());
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

//#define _WITH_PLAYER_TOP

void CGameFramework::FrameAdvance()
{
	HRESULT hResult;

	m_GameTimer.Tick(0.0f);

	ProcessInput();

	AnimateObjects();

	hResult = m_pd3dCommandAllocator->Reset();
	hResult = m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), nullptr);

	::SynchronizeResourceTransition(
		m_pd3dCommandList.Get(),
		m_ppd3dSwapChainBackBuffers[m_nSwapChainBufferIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	m_pScene->OnPrepareRender(m_pd3dCommandList.Get(), m_pCamera);

	if (m_nDrawOption == DRAW_SCENE_COLOR)//'S'
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

		m_pScene->Render(m_pd3dCommandList.Get(), m_pCamera);

		m_pPlayer->Render(m_pd3dCommandList.Get(), m_pCamera);

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

#ifdef _WITH_PRESENT_PARAMETERS
	DXGI_PRESENT_PARAMETERS dxgiPresentParameters;
	dxgiPresentParameters.DirtyRectsCount = 0;
	dxgiPresentParameters.pDirtyRects = nullptr;
	dxgiPresentParameters.pScrollRect = nullptr;
	dxgiPresentParameters.pScrollOffset = nullptr;
	m_pdxgiSwapChain->Present1(1, 0, &dxgiPresentParameters);
#else
#ifdef _WITH_SYNCH_SWAPCHAIN
	m_pdxgiSwapChain->Present(1, 0);
#else
	m_pdxgiSwapChain->Present(0, 0);
#endif
#endif

	MoveToNextFrame();

	m_GameTimer.GetFrameRate(m_pszFrameRate + 12, 37);
	::SetWindowText(m_hWnd, m_pszFrameRate);
}