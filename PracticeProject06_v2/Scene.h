#pragma once
#include "Shader.h"

class CScene
{
public:
	CScene();
	~CScene();

//Build
public:
	void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

	// 그래픽 루트 시그너쳐를 생성한다.
	void CreateGraphicsRootSignature(ID3D12Device* pd3dDevice);
	ComPtr<ID3D12RootSignature> GetGraphicsRootSignature();

// Render
public:
	// 씬에서 마우스와 키보드 메시지를 처리한다.
	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	bool ProcessInput(UCHAR* pKeysBuffer);
	void AnimateObjects(float fTimeElapsed);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList);

protected:
	// 씬은 셰이더들의 집합이다. 셰이더들은 게임 객체들의 집합이다.
	vector<shared_ptr<CShader>> m_ppShaders;
	int m_nShaders = 0;

	ComPtr<ID3D12RootSignature> m_pd3dGraphicsRootSignature = NULL;
};

