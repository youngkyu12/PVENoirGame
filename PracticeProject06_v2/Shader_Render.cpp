#include "stdafx.h"
#include "Shader.h"


void CShader::AnimateObjects(float fTimeElapsed)
{
	for (int j = 0; j < m_nObjects; j++) {
		m_ppObjects[j]->Animate(fTimeElapsed);
	}
}

void CShader::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList)
{
	//파이프라인에 그래픽스 상태 객체를 설정한다. 
	pd3dCommandList->SetPipelineState(m_ppd3dPipelineStates[0].Get());
}

void CShader::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
	OnPrepareRender(pd3dCommandList);

	for (int j = 0; j < m_nObjects; j++) {
		if (m_ppObjects[j]) m_ppObjects[j]->Render(pd3dCommandList);
	}
}