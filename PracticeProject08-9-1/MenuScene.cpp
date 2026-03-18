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

    CreateMainCamera(dev, cmd, nullptr);

    {
        constexpr const wchar_t* kMenuDDS = L"Assets/UI/Test.dds";

        m_menuTex = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 0);
        m_menuTex->LoadTextureFromFile(dev, cmd, kMenuDDS, RESOURCE_TEXTURE2D, 0);

        CScene::m_pDescriptorHeap->CreateShaderResourceViewsOther(
            dev,
            m_menuTex.get(),
            ROOT_PARAMETER_GLOBAL_SRV
        );

        m_menuSrvIndex = m_menuTex->GetSrvIndex(0);
    }

    {
        m_menuShader = std::make_shared<CMenuImageShader>();

        DXGI_FORMAT rtv = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT dsv = DXGI_FORMAT_UNKNOWN;

        m_menuShader->CreateShader(dev, GetGraphicsRootSignature(), 1, &rtv, dsv);
        m_menuShader->CreateShaderVariables(dev, cmd);
    }
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

void CMenuScene::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
    if (!cmd) return;
    if (!camera) camera = m_pMainCamera;

    // Scene 공통 준비: RootSig + DescriptorHeap + Global SRV table + Camera CB
    CScene::OnPrepareRender(cmd, camera);

    if (!m_menuShader) return;
    if (m_menuSrvIndex == UINT_MAX) return;

    PS_CB_DRAW_OPTIONS opt{};
    opt.m_xmn4DrawOptions = XMINT4('T', 0, 0, 0);                // 'T' = gvPostSrvIdx0.x
    opt.m_xmu4PostSrvIdx0 = XMUINT4(m_menuSrvIndex, 0, 0, 0);    // T 슬롯에 메뉴 텍스처 SRV index
    opt.m_xmu4PostSrvIdx1 = XMUINT4(0, 0, 0, 0);

    // Shader.Render()가 내부에서 DrawInstanced(6,1) 호출함
    m_menuShader->Render(cmd, camera, &opt);
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