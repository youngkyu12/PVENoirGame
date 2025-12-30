///-----------------------------------------------------------------------------
// File: Shader_Build.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Shader.h"
#include "DDSTextureLoader12.h"
#include "Scene.h"
#include "Material.h"

void CShader::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext)
{
}

void CShader::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (m_pd3dGraphicsRootSignature)
		pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get());

	if (m_pd3dPipelineState)
		pd3dCommandList->SetPipelineState(m_pd3dPipelineState.Get());
}

void CShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext)
{
	OnPrepareRender(pd3dCommandList);
	UpdateShaderVariables(pd3dCommandList, pContext);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void CPlayerShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext)
{
	CShader::Render(pd3dCommandList, pCamera, pContext);

	if (pCamera)
		pCamera->UpdateShaderVariables(pd3dCommandList);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void CObjectsShader::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext)
{
	UINT ncbElementBytes = ((sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255);
	for (int j = 0; j < m_nObjects; j++)
	{
		CB_GAMEOBJECT_INFO* pbMappedcbGameObject = (CB_GAMEOBJECT_INFO*)((UINT8*)m_pcbMappedGameObjects + (j * ncbElementBytes));
		XMStoreFloat4x4(&pbMappedcbGameObject->m_xmf4x4World, XMMatrixTranspose(XMLoadFloat4x4(&m_ppObjects[j]->m_xmf4x4World)));
#ifdef _WITH_BATCH_MATERIAL
		if (m_pMaterial)
			pbMappedcbGameObject->m_nMaterialID = m_pMaterial->m_nReflection;

		if (m_pMaterial)
			pbMappedcbGameObject->m_nObjectID = j;
#endif
	}
}

void CObjectsShader::AnimateObjects(float fTimeElapsed)
{
	for (int j = 0; j < m_nObjects; j++)
	{
		m_ppObjects[j]->Animate(fTimeElapsed);
	}
}

void CObjectsShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext)
{
	CIlluminatedTexturedShader::Render(pd3dCommandList, pCamera, pContext);

#ifdef _WITH_BATCH_MATERIAL
	if (m_pMaterial)
		m_pMaterial->UpdateShaderVariables(pd3dCommandList);
#endif

	for (int j = 0; j < m_nObjects; j++)
	{
		if (m_ppObjects[j])
			m_ppObjects[j]->Render(pd3dCommandList, pCamera);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void CPostProcessingShader::OnPrepareRenderTarget(
	ID3D12GraphicsCommandList* pd3dCommandList,
	int nRenderTargets,
	D3D12_CPU_DESCRIPTOR_HANDLE* pd3dRtvCPUHandles,
	D3D12_CPU_DESCRIPTOR_HANDLE* pd3dDsvCPUHandle
)
{
	int nResources = m_pTexture->GetTextures();
	unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]> pd3dAllRtvCPUHandles = make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(nRenderTargets + nResources);

	for (int i = 0; i < nRenderTargets; i++)
	{
		pd3dAllRtvCPUHandles[i] = pd3dRtvCPUHandles[i];
		pd3dCommandList->ClearRenderTargetView(pd3dRtvCPUHandles[i], Colors::White, 0, nullptr);
	}

	for (int i = 0; i < nResources; i++)
	{
		::SynchronizeResourceTransition(
			pd3dCommandList,
			GetTextureResource(i),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);

		D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = GetRtvCPUDescriptorHandle(i);
		pd3dCommandList->ClearRenderTargetView(d3dRtvCPUDescriptorHandle, Colors::White, 0, nullptr);
		pd3dAllRtvCPUHandles[nRenderTargets + i] = d3dRtvCPUDescriptorHandle;
	}
	pd3dCommandList->OMSetRenderTargets(nRenderTargets + nResources, pd3dAllRtvCPUHandles.get(), FALSE, pd3dDsvCPUHandle);
}

void CPostProcessingShader::OnPostRenderTarget(ID3D12GraphicsCommandList* pd3dCommandList)
{
	int nResources = m_pTexture->GetTextures();
	for (int i = 0; i < nResources; i++)
	{
		::SynchronizeResourceTransition(
			pd3dCommandList,
			GetTextureResource(i),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COMMON
		);
	}
}

void CPostProcessingShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext)
{
	CShader::Render(pd3dCommandList, pCamera, pContext);

	if (m_pTexture)
		m_pTexture->UpdateShaderVariables(pd3dCommandList);

	pd3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pd3dCommandList->DrawInstanced(6, 1, 0, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void CTextureToFullScreenShader::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext)
{
	m_pcbMappedDrawOptions->m_xmn4DrawOptions.x = *((int*)pContext);
	D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = m_pd3dcbDrawOptions->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(7, d3dGpuVirtualAddress);

	CPostProcessingShader::UpdateShaderVariables(pd3dCommandList, pContext);
}

void CTextureToFullScreenShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext)
{
	CPostProcessingShader::Render(pd3dCommandList, pCamera, pContext);
}