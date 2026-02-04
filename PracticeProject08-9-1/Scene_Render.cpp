//-----------------------------------------------------------------------------
// File: Scene_Render.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"

void CScene::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	::memcpy(m_pcbMappedLights, m_pLights.get(), sizeof(LIGHTS));
	::memcpy(m_pcbMappedMaterials, m_pMaterials.get(), sizeof(MATERIALS));

	// =========================
	// Static batch per-object CB
	// =========================
	if (m_staticBatch.mappedGameObjects && !m_staticBatch.objects.empty())
	{
		const UINT ncb = m_staticBatch.cbElementBytes;
		for (UINT j = 0; j < m_staticBatch.nObjects; ++j)
		{
			if (!m_staticBatch.objects[j]) continue;

			CB_GAMEOBJECT_INFO* cb =
				(CB_GAMEOBJECT_INFO*)((UINT8*)m_staticBatch.mappedGameObjects + (j * ncb));

			XMStoreFloat4x4(
				&cb->m_xmf4x4World,
				XMMatrixTranspose(XMLoadFloat4x4(&m_staticBatch.objects[j]->m_xmf4x4World))
			);

			cb->m_nObjectID = j;
			cb->m_nMaterialID = 0;

#ifdef _WITH_BATCH_MATERIAL
			if (m_staticBatch.material)
				cb->m_nMaterialID = m_staticBatch.material->m_nReflection;

			if (m_staticBatch.material)
				cb->m_nObjectID = j;
#endif
		}
	}

	// =========================
	// Skinned batch per-object CB
	// =========================
	if (m_skinnedBatch.mappedGameObjects && !m_skinnedBatch.objects.empty())
	{
		const UINT ncb = m_skinnedBatch.cbElementBytes;
		for (UINT j = 0; j < m_skinnedBatch.nObjects; ++j)
		{
			if (!m_skinnedBatch.objects[j]) continue;

			CB_GAMEOBJECT_INFO* cb =
				(CB_GAMEOBJECT_INFO*)((UINT8*)m_skinnedBatch.mappedGameObjects + (j * ncb));

			XMStoreFloat4x4(
				&cb->m_xmf4x4World,
				XMMatrixTranspose(XMLoadFloat4x4(&m_skinnedBatch.objects[j]->m_xmf4x4World))
			);

			cb->m_nObjectID = j;
			cb->m_nMaterialID = 0;

#ifdef _WITH_BATCH_MATERIAL
			if (m_skinnedBatch.material)
				cb->m_nMaterialID = m_skinnedBatch.material->m_nReflection;

			if (m_skinnedBatch.material)
				cb->m_nObjectID = j;
#endif
		}
	}
}

bool CScene::ProcessInput(UCHAR* pKeysBuffer)
{
	return(false);
}

void CScene::AnimateObjects(float fTimeElapsed)
{
	// ---- Static batch ----
	for (UINT j = 0; j < m_staticBatch.nObjects; ++j)
	{
		if (j >= m_staticBatch.objects.size()) break;
		if (!m_staticBatch.objects[j]) continue;

		m_staticBatch.objects[j]->Animate(fTimeElapsed);
	}

	// ---- Skinned batch ----
	for (UINT j = 0; j < m_skinnedBatch.nObjects; ++j)
	{
		if (j >= m_skinnedBatch.objects.size()) break;
		if (!m_skinnedBatch.objects[j]) continue;

		m_skinnedBatch.objects[j]->Animate(fTimeElapsed);
	}

	if (m_pLights)
	{
		m_pLights->m_pLights[1].m_xmf3Position = m_pPlayer->GetPosition();
		m_pLights->m_pLights[1].m_xmf3Direction = m_pPlayer->GetLookVector();
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
		m_staticBatch.shader->Render(pd3dCommandList, pCamera, nullptr);

		for (UINT j = 0; j < m_staticBatch.nObjects; ++j)
		{
			if (j >= m_staticBatch.objects.size()) break;
			if (!m_staticBatch.objects[j]) continue;

			m_staticBatch.objects[j]->Render(pd3dCommandList, pCamera);
		}
	}

	// ---- Skinned batch ----
	if (m_skinnedBatch.shader)
	{
		m_skinnedBatch.shader->Render(pd3dCommandList, pCamera, nullptr);

		for (UINT j = 0; j < m_skinnedBatch.nObjects; ++j)
		{
			if (j >= m_skinnedBatch.objects.size()) break;
			if (!m_skinnedBatch.objects[j]) continue;

			m_skinnedBatch.objects[j]->Render(pd3dCommandList, pCamera);
		}
	}
}