#include "stdafx.h"
#include "Object.h"
#include "Shader.h"

void CGameObject::Animate(float fTimeElapsed)
{
}

void CGameObject::OnPrepareRender()
{
}

void CGameObject::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
	OnPrepareRender();

	//게임 객체에 셰이더 객체가 연결되어 있으면 셰이더 상태 객체를 설정한다.
	if (m_pShader) m_pShader->Render(pd3dCommandList);

	//게임 객체에 메쉬가 연결되어 있으면 메쉬를 렌더링한다.
	if (m_pMesh) m_pMesh->Render(pd3dCommandList);
}