#pragma once

#include "Scene.h"
#include "SceneUI.h"

class CMenuScene final : public CScene
{
public:
	CMenuScene() = default;
	~CMenuScene() override = default;

private:
	CSceneUI m_menuUI;
	int m_menuBackgroundSpriteIndex = -1;
	int m_startButtonSpriteIndex = -1;
	int m_loadingSpriteIndex = -1;

public:
	void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) override;
	void ReleaseObjects() override;
	void CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* target) override;

	void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera) override;
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera = NULL) override;

	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam) override;
	bool ConsumeSceneRequest(ESceneRequest& outReq) override;

private:
	bool m_startGameRequested = false;
	bool m_showLoading = false;
};