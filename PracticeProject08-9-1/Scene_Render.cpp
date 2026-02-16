//-----------------------------------------------------------------------------
// File: Scene_Render.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"
#include "LightComponent.h"

void CScene::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	// ============================================================
	// Pack LightComponents -> LIGHTS (scene CB)
	// ============================================================
	if (m_pLights)
	{
		::ZeroMemory(m_pLights.get(), sizeof(LIGHTS));
		m_pLights->m_xmf4GlobalAmbient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);

		UINT li = 0;
		for (auto& obj : m_lightObjects)
		{
			if (!obj) continue;
			auto* lc = obj->GetComponent<CLightComponent>();
			if (!lc) continue;
			if (!lc->IsEnabled()) continue;
			if (li >= MAX_LIGHTS) break;

			lc->Fill(m_pLights->m_pLights[li]);
			++li;
		}
	}

	::memcpy(m_pcbMappedLights, m_pLights.get(), sizeof(LIGHTS));
	::memcpy(m_pcbMappedMaterials, m_pMaterials.get(), sizeof(MATERIALS));

	// =========================
	// Static batch per-object CB
	// =========================
	if (m_staticBatch.mappedGameObjects && !m_staticBatch.objectRefs.empty())
	{
		const UINT ncb = m_staticBatch.cbElementBytes;

		for (UINT j = 0; j < (UINT)m_staticBatch.objectRefs.size(); ++j)
		{
			auto* obj = m_staticBatch.objectRefs[j];
			if (!obj) continue;

			auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)m_staticBatch.mappedGameObjects + j * ncb);

			const XMFLOAT4X4& W = obj->GetWorldMatrix(); // Transform에서 가져와야 함

			XMStoreFloat4x4(
				&cb->m_xmf4x4World,
				XMMatrixTranspose(XMLoadFloat4x4(&W))
			);

			cb->m_nObjectID = j;
		}
	}

	// =========================
	// Skinned batch per-object CB
	// =========================
	if (m_skinnedBatch.mappedGameObjects && !m_skinnedBatch.objectRefs.empty())
	{
		const UINT ncb = m_skinnedBatch.cbElementBytes;

		for (UINT j = 0; j < (UINT)m_skinnedBatch.objectRefs.size(); ++j)
		{
			auto* obj = m_skinnedBatch.objectRefs[j];
			if (!obj) continue;

			auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)m_skinnedBatch.mappedGameObjects + j * ncb);

			const XMFLOAT4X4& W = obj->GetWorldMatrix(); // Transform에서 가져와야 함

			XMStoreFloat4x4(
				&cb->m_xmf4x4World,
				XMMatrixTranspose(XMLoadFloat4x4(&W))
			);

			cb->m_nObjectID = j;
		}
	}
	// =========================
	// Player per-object CB (single)
	// =========================
	if (m_pPlayer && m_pcbMappedPlayerGameObject)
	{
		const XMFLOAT4X4& W = m_pPlayer->GetWorldMatrix();

		XMStoreFloat4x4(
			&m_pcbMappedPlayerGameObject->m_xmf4x4World,
			XMMatrixTranspose(XMLoadFloat4x4(&W))
		);

		m_pcbMappedPlayerGameObject->m_nObjectID = 0; // 필요하면 플레이어 ID로
	}

}

bool CScene::ProcessInput(UCHAR* pKeysBuffer)
{
	return(false);
}

void CScene::AnimateObjects(float fTimeElapsed)
{
	for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
	{
		if (!m_staticObjects[j]) continue;
		m_staticObjects[j]->Animate(fTimeElapsed);
	}

	for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
	{
		if (!m_skinnedObjects[j]) continue;
		m_skinnedObjects[j]->Animate(fTimeElapsed);
	}
	if (m_pPlayer)
		m_pPlayer->Animate(fTimeElapsed);

	// Player spot light follower target set (once)
	if (m_pPlayer && m_pPlayerSpotFollower && (m_pPlayerSpotFollower->GetTarget() == nullptr))
	{
		m_pPlayerSpotFollower->SetTarget(m_pPlayer.get());
	}

	// Update light objects (FollowTransformComponent 포함)
	for (UINT j = 0; j < (UINT)m_lightObjects.size(); ++j)
	{
		if (!m_lightObjects[j]) continue;
		m_lightObjects[j]->Animate(fTimeElapsed);
	}

}


void CScene::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get());
	pd3dCommandList->SetDescriptorHeaps(1, m_pDescriptorHeap->m_pd3dCbvSrvDescriptorHeap.GetAddressOf());

	pd3dCommandList->SetGraphicsRootDescriptorTable(
		ROOT_PARAMETER_GLOBAL_SRV,
		m_pDescriptorHeap->GetGPUSrvDescriptorStartHandle()
	);

	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pCamera->UpdateShaderVariables(pd3dCommandList);

	UpdateShaderVariables(pd3dCommandList);

	D3D12_GPU_VIRTUAL_ADDRESS d3dcbLightsGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_LIGHT, d3dcbLightsGpuVirtualAddress); //Lights

	D3D12_GPU_VIRTUAL_ADDRESS d3dcbMaterialsGpuVirtualAddress = m_pd3dcbMaterials->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_MATERIAL, d3dcbMaterialsGpuVirtualAddress); //Materials
}

void CScene::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
    // ---- Static batch ----
    if (m_staticBatch.shader)
    {
		m_staticBatch.shader->Render(pd3dCommandList, pCamera, &m_staticBatch);
		for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
		{
			if (!m_staticObjects[j]) continue;
			m_staticObjects[j]->Render(pd3dCommandList, pCamera);
		}
    }

    // ---- Skinned batch ----
    if (m_skinnedBatch.shader)
    {
		m_skinnedBatch.shader->Render(pd3dCommandList, pCamera, &m_skinnedBatch);
		for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
		{
			if (!m_skinnedObjects[j]) continue;
			m_skinnedObjects[j]->Render(pd3dCommandList, pCamera);
		}
    }

    // ---- Player ----
	if (m_pPlayer)
		m_pPlayer->Render(pd3dCommandList, pCamera);
}
