//-----------------------------------------------------------------------------
// File: Scene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"
#include "AnimatorComponent.h"
#include "AnimController.h"

std::unique_ptr<CDescriptorHeap> CScene::m_pDescriptorHeap = std::make_unique<CDescriptorHeap>();
;
CScene::CScene()
{
	m_staticBatch.capacity = 4;
	m_staticBatch.count = 0;

	m_skinnedBatch.capacity = 10;
	m_skinnedBatch.count = 0;

	m_demoFighters.fill(nullptr);
}

CScene::~CScene()
{
}

CGameObject* CScene::GetDemoFighter(int index) const
{
	if (index < 0 || index >= (int)m_demoFighters.size()) return nullptr;
	return m_demoFighters[(size_t)index];
}

void CScene::RequestDemoFighterAttack(int index)
{
	CGameObject* obj = GetDemoFighter(index);
	if (!obj) return;

	// 1) 컴포넌트 기반(향후 서버/상태동기화 확장용)
	if (auto* animComp = obj->GetComponent<CAnimatorComponent>())
	{
		if (auto* ctrl = animComp->EnsureController())
			ctrl->RequestAttack();
		return;
	}

	// 2) 레거시 기반(혼용/호환)
	if (auto* ctrl = obj->GetAnimController())
		ctrl->RequestAttack();
}

void CScene::ReleaseObjects()
{
	if (m_pd3dGraphicsRootSignature)
		m_pd3dGraphicsRootSignature.Reset();

	// shaders: shared_ptr reset만(필요하면)
	m_staticBatch.shader.reset();
	m_skinnedBatch.shader.reset();

	// objects clear
	m_staticObjects.clear();
	m_skinnedObjects.clear();
	m_pPlayer.reset();

	m_lightObjects.clear();
	m_pPlayerSpotFollower = nullptr;

	m_demoFighters.fill(nullptr);

	// Main Camera (GameObject + Camera Component)
	if (m_pMainCamera)
	{
		m_pMainCamera->ReleaseShaderVariables();
		m_pMainCamera = nullptr;
	}
	m_pMainCameraObject.reset();


	// Batch ref lists clear
	m_staticBatch.objectRefs.clear();
	m_skinnedBatch.objectRefs.clear();


	// scene-owned CB/리소스 해제
	ReleaseShaderVariables();
}

void CScene::ReleaseUploadBuffers()
{
	for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
	{
		if (!m_staticObjects[j]) continue;
		m_staticObjects[j]->ReleaseUploadBuffers();
	}
	for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
	{
		if (!m_skinnedObjects[j]) continue;
		m_skinnedObjects[j]->ReleaseUploadBuffers();
	}
	if (m_pPlayer)
		m_pPlayer->ReleaseUploadBuffers();


#ifdef _WITH_BATCH_MATERIAL
	if (m_staticBatch.material)  m_staticBatch.material->ReleaseUploadBuffers();
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

	// ---- Skinned batch CBs ----	if (m_skinnedBatch.cbGameObjects)
	{
		if (m_skinnedBatch.mappedGameObjects)
		{
			m_skinnedBatch.cbGameObjects->Unmap(0, NULL);
			m_skinnedBatch.mappedGameObjects = nullptr;
		}
		m_skinnedBatch.cbGameObjects.Reset();
	}

	if (m_pd3dcbLights)
	{
		m_pd3dcbLights->Unmap(0, NULL);
		m_pd3dcbLights.Reset();
	}
	m_pcbMappedLights = nullptr;

	if (m_pd3dcbMaterials)
	{
		m_pd3dcbMaterials->Unmap(0, NULL);
		m_pd3dcbMaterials.Reset();
	}

	if (m_pd3dcbPlayerGameObject)
	{
		if (m_pcbMappedPlayerGameObject)
		{
			m_pd3dcbPlayerGameObject->Unmap(0, NULL);
			m_pcbMappedPlayerGameObject = nullptr;
		}
		m_pd3dcbPlayerGameObject.Reset();
	}

}
