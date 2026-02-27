//-----------------------------------------------------------------------------
// File: MenuScene.h
//-----------------------------------------------------------------------------

#pragma once
#include "Scene.h"

class CMenuScene final : public CScene
{
public:
    CMenuScene() = default;
    ~CMenuScene() override = default;

    void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) override;
    void CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* target) override;

    // MenuScene은 실체 오브젝트 렌더 안 함
    void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera) override;
    void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera = NULL) override;

    // 클릭 시 GameScene 전환 요청
    bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam) override;

    bool ConsumeSceneRequest(ESceneRequest& outReq) override;

private:
    bool m_startGameRequested = false;
};