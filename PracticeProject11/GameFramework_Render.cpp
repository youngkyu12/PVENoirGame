#include "stdafx.h"
#include "GameFramework.h"

void CGameFramework::ProcessInput()
{
	static UCHAR pKeyBuffer[256];
	DWORD dwDirection = 0;

	// 키보드의 상태 정보를 반환한다. 
	// 화살표 키('→', '←', '↑', '↓')를 누르면 플레이어를 오른쪽/왼쪽(로컬 x-축), 앞/뒤(로컬 z-축)로 이동한다. 
	// 'Page Up'과 'Page Down' 키를 누르면 플레이어를 위/아래(로컬 y-축)로 이동한다.
	if (::GetKeyboardState(pKeyBuffer)) {
		if (pKeyBuffer[VK_UP] & 0xF0) dwDirection |= DIR_FORWARD;
		if (pKeyBuffer[VK_DOWN] & 0xF0) dwDirection |= DIR_BACKWARD;
		if (pKeyBuffer[VK_LEFT] & 0xF0) dwDirection |= DIR_LEFT;
		if (pKeyBuffer[VK_RIGHT] & 0xF0) dwDirection |= DIR_RIGHT;
		if (pKeyBuffer[VK_PRIOR] & 0xF0) dwDirection |= DIR_UP;
		if (pKeyBuffer[VK_NEXT] & 0xF0) dwDirection |= DIR_DOWN;
	}
	float cxDelta = 0.0f, cyDelta = 0.0f;
	POINT ptCursorPos;
	// 마우스를 캡쳐했으면 마우스가 얼마만큼 이동하였는 가를 계산한다. 
	// 마우스 왼쪽 또는 오른쪽 버튼이 눌러질 때의 메시지(WM_LBUTTONDOWN, WM_RBUTTONDOWN)를 처리할 때 마우스를 캡쳐하였다. 
	// 그러므로 마우스가 캡쳐된 것은 마우스 버튼이 눌려진 상태를 의미한다. 
	// 마우스 버튼이 눌려진 상태에서 마우스를 좌우 또는 상하로 움직이면 플레이어를 x-축 또는 y-축으로 회전한다.
	if (::GetCapture() == m_hWnd) {
		//마우스 커서를 화면에서 없앤다(보이지 않게 한다).
		::SetCursor(NULL);
		//현재 마우스 커서의 위치를 가져온다.
		::GetCursorPos(&ptCursorPos);
		//마우스 버튼이 눌린 상태에서 마우스가 움직인 양을 구한다.
		cxDelta = (float)(ptCursorPos.x - m_ptOldCursorPos.x) / 3.0f;
		cyDelta = (float)(ptCursorPos.y - m_ptOldCursorPos.y) / 3.0f;
		//마우스 커서의 위치를 마우스가 눌려졌던 위치로 설정한다.
		::SetCursorPos(m_ptOldCursorPos.x, m_ptOldCursorPos.y);
	}
	//마우스 또는 키 입력이 있으면 플레이어를 이동하거나(dwDirection) 회전한다(cxDelta 또는 cyDelta).
	if ((dwDirection != 0) || (cxDelta != 0.0f) || (cyDelta != 0.0f)) {
		if (cxDelta || cyDelta) {
			// cxDelta는 y-축의 회전을 나타내고 cyDelta는 x-축의 회전을 나타낸다. 
			// 오른쪽 마우스 버튼이 눌려진 경우 cxDelta는 z-축의 회전을 나타낸다.
			if (pKeyBuffer[VK_RBUTTON] & 0xF0)
				m_pPlayer->Rotate(cyDelta, 0.0f, -cxDelta);
			else
				m_pPlayer->Rotate(cyDelta, cxDelta, 0.0f);
		}
		// 플레이어를 dwDirection 방향으로 이동한다(실제로는 속도 벡터를 변경한다). 
		// 이동 거리는 시간에 비례하도록 한다.
		// 플레이어의 이동 속력은 (50/초)로 가정한다.
		if (dwDirection) m_pPlayer->Move(dwDirection, 50.0f * m_GameTimer.GetTimeElapsed(), true);
	}

	//플레이어를 실제로 이동하고 카메라를 갱신한다. 중력과 마찰력의 영향을 속도 벡터에 적용한다.
	m_pPlayer->Update(m_GameTimer.GetTimeElapsed());
}

void CGameFramework::AnimateObjects()
{
	if (m_pScene) 
		m_pScene->AnimateObjects(m_GameTimer.GetTimeElapsed());
}

void CGameFramework::MoveToNextFrame()
{
	m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();

	UINT64 nFenceValue = ++m_nFenceValues[m_nSwapChainBufferIndex];
	HRESULT hResult = m_pd3dCommandQueue->Signal(m_pd3dFence.Get(), nFenceValue);
	if (m_pd3dFence->GetCompletedValue() < nFenceValue) {
		hResult = m_pd3dFence->SetEventOnCompletion(nFenceValue, m_hFenceEvent);
		::WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}

#define _WITH_PLAYER_TOP

void CGameFramework::FrameAdvance()
{
	m_GameTimer.Tick(0.0f);

	ProcessInput();

	AnimateObjects();

	HRESULT hResult = m_pd3dCommandAllocator->Reset();
	hResult = m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), NULL);

	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource = m_ppd3dSwapChainBackBuffers[m_nSwapChainBufferIndex].Get();
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (m_nSwapChainBufferIndex * m_nRtvDescriptorIncrementSize);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle = m_pd3dDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	float pfClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
	m_pd3dCommandList->ClearRenderTargetView(
		d3dRtvCPUDescriptorHandle,
		pfClearColor/*Colors::Azure*/,
		0,
		NULL
	);

	m_pd3dCommandList->ClearDepthStencilView(
		d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f,
		0,
		0,
		NULL
	);

	m_pd3dCommandList->OMSetRenderTargets(
		1,
		&d3dRtvCPUDescriptorHandle,
		TRUE,
		&d3dDsvCPUDescriptorHandle
	);

	if (m_pScene) m_pScene->Render(m_pd3dCommandList.Get(), m_pCamera);
#ifdef _WITH_PLAYER_TOP
	//렌더 타겟은 그대로 두고 깊이 버퍼를 1.0으로 지우고 플레이어를 렌더링하면 플레이어는 무조건 그려질 것이다.
	m_pd3dCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
#endif
	if (m_pPlayer) m_pPlayer->Render(m_pd3dCommandList.Get(), m_pCamera);

	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	hResult = m_pd3dCommandList->Close();

	ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get()};
	m_pd3dCommandQueue->ExecuteCommandLists(_countof(ppd3dCommandLists), ppd3dCommandLists);

	WaitForGpuComplete();

	DXGI_PRESENT_PARAMETERS dxgiPresentParameters;
	dxgiPresentParameters.DirtyRectsCount = 0;
	dxgiPresentParameters.pDirtyRects = NULL;
	dxgiPresentParameters.pScrollRect = NULL;
	dxgiPresentParameters.pScrollOffset = NULL;
	m_pdxgiSwapChain->Present1(1, 0, &dxgiPresentParameters);

	MoveToNextFrame();

	m_GameTimer.GetFrameRate(m_pszFrameRate + 12, 37);
	::SetWindowText(m_hWnd, m_pszFrameRate);
}

