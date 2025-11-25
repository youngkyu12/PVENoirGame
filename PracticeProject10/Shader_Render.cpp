#include "stdafx.h"
#include "Shader.h"


void CShader::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList)
{
	pd3dCommandList->SetPipelineState(m_ppd3dPipelineStates[0].Get());
}

void CShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	OnPrepareRender(pd3dCommandList);
}

//------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------

void CObjectsShader::AnimateObjects(float fTimeElapsed)
{
	for (int i = 0; i < m_nObjects; ++i)
	{
		m_ppObjects[i]->Animate(fTimeElapsed);
	}
}

void CObjectsShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	CShader::Render(pd3dCommandList, pCamera);
	for (int i = 0; i < m_nObjects; ++i)
	{
		if (m_ppObjects[i])
		{
			m_ppObjects[i]->Render(pd3dCommandList, pCamera);
		}
	}
}
