//-----------------------------------------------------------------------------
// File: Scene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"

std::unique_ptr<CDescriptorHeap> CScene::m_pDescriptorHeap = std::make_unique<CDescriptorHeap>();

CScene::CScene()
{
	m_staticBatch.nObjects = 1;
	m_skinnedBatch.nObjects = 60;
}

CScene::~CScene()
{
}

void CScene::ReleaseObjects()
{
	if (m_pd3dGraphicsRootSignature)
		m_pd3dGraphicsRootSignature.Reset();

	if (!m_ppShaders.empty())
	{
		for (int i = 0; i < m_nShaders; i++)
		{
			m_ppShaders[i]->ReleaseShaderVariables();
			m_ppShaders[i]->ReleaseObjects();
		}
	}
	m_staticBatch.objects.clear();
	m_skinnedBatch.objects.clear();
	ReleaseShaderVariables();
}

void CScene::ReleaseUploadBuffers()
{
	// ---- Static batch objects ----
	for (UINT j = 0; j < m_staticBatch.nObjects; ++j)
	{
		if (j >= m_staticBatch.objects.size()) break;
		if (!m_staticBatch.objects[j]) continue;
		m_staticBatch.objects[j]->ReleaseUploadBuffers();
	}

	// ---- Skinned batch objects ----
	for (UINT j = 0; j < m_skinnedBatch.nObjects; ++j)
	{
		if (j >= m_skinnedBatch.objects.size()) break;
		if (!m_skinnedBatch.objects[j]) continue;
		m_skinnedBatch.objects[j]->ReleaseUploadBuffers();
	}

#ifdef _WITH_BATCH_MATERIAL
	if (m_staticBatch.material)  m_staticBatch.material->ReleaseUploadBuffers();
	if (m_skinnedBatch.material) m_skinnedBatch.material->ReleaseUploadBuffers();
#endif
}


void CScene::ReleaseShaderVariables()
{
	// ---- Static batch CB ----
	if (m_staticBatch.cbGameObjects)
	{
		if (m_staticBatch.mappedGameObjects)
		{
			m_staticBatch.cbGameObjects->Unmap(0, NULL);
			m_staticBatch.mappedGameObjects = nullptr;
		}
		m_staticBatch.cbGameObjects.Reset();
	}

	// ---- Skinned batch CBs ----
	if (m_skinnedBatch.cbGameObjects)
	{
		if (m_skinnedBatch.mappedGameObjects)
		{
			m_skinnedBatch.cbGameObjects->Unmap(0, NULL);
			m_skinnedBatch.mappedGameObjects = nullptr;
		}
		m_skinnedBatch.cbGameObjects.Reset();
	}

	if (m_skinnedBatch.cbBonePalette)
	{
		if (m_skinnedBatch.mappedBonePalette)
		{
			m_skinnedBatch.cbBonePalette->Unmap(0, NULL);
			m_skinnedBatch.mappedBonePalette = nullptr;
		}
		m_skinnedBatch.cbBonePalette.Reset();
	}
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
