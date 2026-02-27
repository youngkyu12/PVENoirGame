//-----------------------------------------------------------------------------
// File: MenuScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "MenuScene.h"

#include "Object.h"
#include "Camera.h"

void CMenuScene::BuildObjects(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
    CreateGraphicsRootSignature(dev);

    constexpr UINT MAX_GLOBAL_SRVS = 1024;
    const UINT cbvTotal = 32;

    CScene::m_pDescriptorHeap->CreateCbvSrvDescriptorHeaps(
        dev,
        cbvTotal,
        MAX_GLOBAL_SRVS
    );

    CreateMainCamera(dev, cmd, nullptr);
}

// MenuScene 카메라: "아무 위치에서 아무대나" 보는 임시 카메라
void CMenuScene::CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* /*target*/)
{
    m_pMainCameraObject = std::make_unique<CGameObject>(0);

    auto* cam = m_pMainCameraObject->AddComponent<CThirdPersonCamera>();
    m_pMainCamera = cam;

    cam->SetMode(THIRD_PERSON_CAMERA);
    cam->SetTarget(nullptr);

    cam->SetTimeLag(0.0f);
    cam->SetOffset(XMFLOAT3(0.0f, 0.0f, 0.0f));

    cam->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
    cam->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
    cam->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);

    m_pMainCameraObject->CreateComponents(dev, cmd);

    // Update/SetLookAt은 XMFLOAT3& 요구(비 const) + 타겟 없으면 의미도 없음
    // 대신 ViewMatrix를 직접 만든다.
    XMFLOAT3 pos(0.0f, 2.0f, -5.0f);
    XMFLOAT3 lookAt(0.0f, 0.5f, 0.0f);
    XMFLOAT3 up(0.0f, 1.0f, 0.0f);

    cam->SetPosition(pos);
    cam->SetLookAtPosition(lookAt);
    cam->GenerateViewMatrix(pos, lookAt, up);
}

void CMenuScene::OnPrepareRender(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
    CScene::OnPrepareRender(cmd, camera);
}

void CMenuScene::Render(ID3D12GraphicsCommandList* /*pd3dCommandList*/, CCamera* /*pCamera*/)
{
    // 의도적으로 아무 것도 그리지 않음
}

bool CMenuScene::OnProcessingMouseMessage(HWND /*hWnd*/, UINT nMessageID, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    if (nMessageID == WM_LBUTTONDOWN)
    {
        m_startGameRequested = true;
        return true;
    }
    return false;
}

bool CMenuScene::ConsumeSceneRequest(ESceneRequest& outReq)
{
    if (!m_startGameRequested)
    {
        outReq = ESceneRequest::None;
        return false;
    }

    m_startGameRequested = false;
    outReq = ESceneRequest::SwitchToGame;
    return true;
}