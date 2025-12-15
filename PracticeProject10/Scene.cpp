#include "stdafx.h"
#include "Scene.h"


void CScene::ReleaseObjects()
{
	if (m_pd3dGraphicsRootSignature) m_pd3dGraphicsRootSignature.Reset();
	for (int i = 0; i < m_nShaders; ++i)
		m_ppShaders[i]->ReleaseObjects();
}

void CScene::ReleaseUploadBuffers()
{
	for (int i = 0; i < m_nShaders; ++i) 
		m_ppShaders[i]->ReleaseUploadBuffers();
}
