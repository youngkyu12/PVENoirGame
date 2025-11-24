#pragma once
#include "Shader.h"

class CScene {
public:
	CScene();
	virtual ~CScene();

//Build
public:
	void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

	void CreateGraphicsRootSignature(ID3D12Device* pd3dDevice);

// Render
public:
	bool ProcessInput(UCHAR* pKeysBuffer);
	void AnimateObjects(float fTimeElapsed);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList);

// Input
public:
	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

// protected member variables
protected:
	vector<shared_ptr<CShader>> m_ppShaders;
	int m_nShader = 0;

	ComPtr<ID3D12RootSignature> m_pd3dGraphicsRootSignature = NULL;
};

