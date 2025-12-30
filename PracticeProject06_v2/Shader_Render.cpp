#include "stdafx.h"
#include "Shader.h"


void CShader::AnimateObjects(float fTimeElapsed)
{
	for (int i = 0; i < m_nObject; ++i) {
		m_ppObjects[i]->Animate(fTimeElapsed);
	}
}

void CShader::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList)
{
	pd3dCommandList->SetPipelineState(m_ppd3dPipelineStates[0].Get());
}

void CShader::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
	OnPrepareRender(pd3dCommandList);

	for (int i = 0; i < m_nObject; ++i) {
		if (m_ppObjects[i]) m_ppObjects[i]->Render(pd3dCommandList);
	}
}