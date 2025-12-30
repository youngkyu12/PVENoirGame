#pragma once
#include "BuildResource.h"

class Scene_Build
{
public:
	Scene_Build(shared_ptr<BuildRes> buildRes);
	~Scene_Build() {}

	void CreateGraphicsRootSignature(ID3D12Device* pd3dDevice);
	void CreateGraphicsPipelineState(ID3D12Device* pd3dDevice);

	void BuildObjects(ID3D12Device* pd3dDevice);

protected:
	shared_ptr<BuildRes> m_buildRes;


};

class Scene_Render
{
public:
	Scene_Render(shared_ptr<BuildRes> buildRes);
	~Scene_Render() {}

	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void ReleaseObjects();

	bool ProcessInput();
	void AnimateObjects(float fTimeElapsed);
	void PrepareRender(ID3D12GraphicsCommandList* pd3dCommandList);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList);

protected:
	shared_ptr<BuildRes> m_buildRes;
};