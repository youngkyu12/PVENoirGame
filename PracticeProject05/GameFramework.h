#pragma once
#include "BuildResource.h"
#include "Scene.h"

class GameFramework_Build
{
public:
	GameFramework_Build(shared_ptr<BuildRes> buildRes);
	~GameFramework_Build();

	bool OnCreate(HINSTANCE hInstance, HWND hMainWnd);
	void OnDestroy();
	// 프레임워크를 초기화하는 함수이다(주 윈도우가 생성되면 호출된다). 

	void CreateDirect3DDevice();
	void CreateFence();
	void CreateCommandQueueAndList();
	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateRenderTargetViews();
	void CreateDepthStencilView();
	// 스왑 체인, 디바이스, 서술자 힙, 명령 큐/할당자/리스트를 생성하는 함수이다.     

	void BuildObjects();
	void ReleaseObjects();

protected:
	shared_ptr<BuildRes> m_buildRes;
	unique_ptr<Scene_Build> m_pScene;
};

class GameFramework_Render
{
public:
	GameFramework_Render(shared_ptr<BuildRes> buildRes);
	~GameFramework_Render();

	void OnDestroy();
	void ReleaseObjects();

	virtual void ChangeSwapChainState();
	virtual void OnResizeBackBuffers();

	void ProcessInput();
	void AnimateObjects();
	void FrameAdvance();

	void WaitForGpuComplete();
	void MoveToNextFrame();

	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM	lParam);
	void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void CreateRenderTargetViews();
	void CreateDepthStencilView();

protected:
	shared_ptr<BuildRes> m_buildRes;
	unique_ptr<Scene_Render> m_pScene;
	_TCHAR m_pszFrameRate[50]{ 0 };
};
