//-----------------------------------------------------------------------------
// File: MenuScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "MenuScene.h"

#include "Object.h"
#include "Camera.h"

// MenuScene: 리소스는 "최소한"으로만 만든다.
// - RootSignature는 PostProcess가 필요로 하므로 생성
// - DescriptorHeap(CBV/SRV)은 PostProcess SRV 생성/바인딩에 필요하므로 생성
// - 실제 렌더 오브젝트/배치/플레이어는 만들지 않는다.
void CMenuScene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
    // 1) 루트 시그니처는 기존 Scene과 동일 레이아웃 유지
    CreateGraphicsRootSignature(pd3dDevice);

    // 2) 디스크립터 힙은 최소한만 확보 (PostProcess SRV/CBV가 여기로 들어간다)
    constexpr UINT MAX_GLOBAL_SRVS = 1024;

    // CBV 슬롯은 넉넉하게(카메라/기타가 추가로 쓰더라도 안전)
    const UINT cbvTotal = 32;

    CScene::m_pDescriptorHeap->CreateCbvSrvDescriptorHeaps(
        pd3dDevice,
        cbvTotal,
        MAX_GLOBAL_SRVS
    );

    // 3) MenuScene은 아무 오브젝트도 만들지 않는다.
    m_staticBatch.shader.reset();
    m_skinnedBatch.shader.reset();

    m_staticBatch.objectRefs.clear();
    m_skinnedBatch.objectRefs.clear();

    m_staticObjects.clear();
    m_skinnedObjects.clear();
    m_lightObjects.clear();

    m_pPlayer.reset();
    m_demoFighters.fill(nullptr);
    m_arrowRefs.clear();
    m_pPlayerSpotFollower = nullptr;

    // 4) 카메라만 만든다(타겟 없음)
    CreateMainCamera(pd3dDevice, pd3dCommandList, nullptr);
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

// MenuScene은 Scene의 무거운 조명/재질 업데이트를 하지 않는다.
// PostProcess가 정상 동작하도록 "RootSignature + DescriptorHeap + Camera"만 세팅한다.
void CMenuScene::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
    pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get());
    pd3dCommandList->SetDescriptorHeaps(1, CScene::m_pDescriptorHeap->m_pd3dCbvSrvDescriptorHeap.GetAddressOf());

    pd3dCommandList->SetGraphicsRootDescriptorTable(
        ROOT_PARAMETER_GLOBAL_SRV,
        CScene::m_pDescriptorHeap->GetGPUSrvDescriptorStartHandle()
    );

    if (pCamera)
    {
        pCamera->SetViewportsAndScissorRects(pd3dCommandList);
        pCamera->UpdateShaderVariables(pd3dCommandList);
    }
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