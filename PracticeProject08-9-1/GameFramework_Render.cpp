//-----------------------------------------------------------------------------
// File: CGameFramework_Render.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameFramework.h"
#include "AnimController.h"
#include "AnimatorComponent.h"
#include "PlayerControllerComponent.h"


void CGameFramework::ProcessInput()
{
	static UCHAR pKeysBuffer[256];
	bool bProcessedByScene = false;

	if (GetKeyboardState(pKeysBuffer) && m_pScene)
		bProcessedByScene = m_pScene->ProcessInput(pKeysBuffer);

	CGameObject* playerObj = (m_pScene ? m_pScene->GetPlayer() : nullptr);
	if (!playerObj) return;
	if (m_ptOldCursorPos.x == 0 && m_ptOldCursorPos.y == 0)
		::GetCursorPos(&m_ptOldCursorPos);


	auto* pc = playerObj->GetComponent<CPlayerControllerComponent>();
	if (!pc) return;

	DWORD dwDirection = 0;
	float cxDelta = 0.0f, cyDelta = 0.0f;

	if (!bProcessedByScene)
	{
		if (pKeysBuffer[VK_UP] & 0xF0)    dwDirection |= DIR_FORWARD;
		if (pKeysBuffer[VK_DOWN] & 0xF0)  dwDirection |= DIR_BACKWARD;
		if (pKeysBuffer[VK_LEFT] & 0xF0)  dwDirection |= DIR_LEFT;
		if (pKeysBuffer[VK_RIGHT] & 0xF0) dwDirection |= DIR_RIGHT;
		if (pKeysBuffer[VK_PRIOR] & 0xF0) dwDirection |= DIR_UP;
		if (pKeysBuffer[VK_NEXT] & 0xF0)  dwDirection |= DIR_DOWN;

		POINT ptCursorPos;
		GetCursorPos(&ptCursorPos);

		cxDelta = (float)(ptCursorPos.x - m_ptOldCursorPos.x) / 3.0f;
		cyDelta = (float)(ptCursorPos.y - m_ptOldCursorPos.y) / 3.0f;

		m_ptOldCursorPos = ptCursorPos;

	}

	const float dt = m_GameTimer.GetTimeElapsed();

	// === delta 계산을 위해 이동 전 위치 저장 ===
	XMFLOAT3 oldPos = playerObj->GetPosition();

	// === Rotate/Move: "Player 전용 호출" 금지, 컨트롤러로만 ===
	if (cxDelta || cyDelta)
	{
		if (pKeysBuffer[VK_RBUTTON] & 0xF0)
			pc->Rotate(cyDelta, 0.0f, -cxDelta);
		else
			pc->Rotate(cyDelta, cxDelta, 0.0f);
	}

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