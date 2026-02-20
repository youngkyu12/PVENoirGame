//-----------------------------------------------------------------------------
// File: Scene_Input.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"
#include "Object.h"
#include "AnimController.h"

bool CScene::OnProcessingMouseMessage(HWND /*hWnd*/, UINT nMessageID, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    if (nMessageID == WM_LBUTTONDOWN)
    {
        if (m_pPlayer)
        {
            // 1) AnimController가 있으면 Attack 요청
            if (auto* ctrl = m_pPlayer->GetAnimController())
            {
                ctrl->RequestAttack();
                return true; // 처리했음
            }

            // 2) AnimatorComponent 기반이면 (프로젝트에서 쓰는 경우) 여기서 추가 분기 가능
            //    (현재 첨부 코드상 GetAnimController 경로가 이미 존재하므로 우선 생략)
        }
    }
    return false;
}


bool CScene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	return(false);
}