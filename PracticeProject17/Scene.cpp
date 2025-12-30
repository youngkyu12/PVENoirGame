#include "stdafx.h"
#include "Scene.h"

void CScene::ReleaseObjects()
{
	if (m_pd3dGraphicsRootSignature) m_pd3dGraphicsRootSignature.Reset();
	for (int i = 0; i < m_nShaders; ++i)
		m_ppShaders[i]->ReleaseObjects();
	ReleaseShaderVariables();
}

void CScene::ReleaseUploadBuffers()
{
	for (int i = 0; i < m_nShaders; ++i)
		m_ppShaders[i]->ReleaseUploadBuffers();
}

void CScene::ReleaseShaderVariables()
{
	if (m_pd3dcbLights)
	{
		m_pd3dcbLights->Unmap(0, NULL);
		m_pd3dcbLights.Reset();
	}
	if (m_pd3dcbMaterials)
	{
		m_pd3dcbMaterials->Unmap(0, NULL);
		m_pd3dcbMaterials.Reset();
	}
}
