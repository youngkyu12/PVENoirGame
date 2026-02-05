///-----------------------------------------------------------------------------
// File: Shader_Render.cpp
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

void CPlayerShader::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext)
{
	// CShader::Render() 내부에서 OnPrepareRender(루트시그/PSO 설정) 후 호출되므로
	// 여기서 BonePalette 루트 바인딩을 하면 순서가 안전함.
	if (m_pd3dcbBonePalette)
	{
		pd3dCommandList->SetGraphicsRootConstantBufferView(
			ROOT_PARAMETER_BONE_PALETTE,
			m_pd3dcbBonePalette->GetGPUVirtualAddress()
		);
	}

	CShader::UpdateShaderVariables(pd3dCommandList, pContext);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void CStaticObjectsShader::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext)
{
	(void)pd3dCommandList;
	(void)pContext;
}

void CStaticObjectsShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext)
{
	CIlluminatedTexturedShader::Render(pd3dCommandList, pCamera, pContext);

#ifdef _WITH_BATCH_MATERIAL
	if (m_pBatch && m_pBatch->material && m_pBatch->material->NeedsLegacyBinding())
		m_pBatch->material->UpdateShaderVariables(pd3dCommandList);
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void CSkinnedObjectsShader::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext)
{
	(void)pd3dCommandList;
	(void)pContext;
}

void CSkinnedObjectsShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext)
{
	CIlluminatedTexturedShader::Render(pd3dCommandList, pCamera, pContext);
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
		pd3dCommandList->ClearRenderTargetView(pd3dRtvCPUHandles[i], Colors::Black, 0, nullptr);
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

	//if (m_pTexture)
	//	m_pTexture->UpdateShaderVariables(pd3dCommandList);

	pd3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pd3dCommandList->DrawInstanced(6, 1, 0, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void CTextureToFullScreenShader::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext)
{
	m_pcbMappedDrawOptions->m_xmn4DrawOptions.x = *((int*)pContext);
	D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = m_pd3dcbDrawOptions->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(5, d3dGpuVirtualAddress);

	CPostProcessingShader::UpdateShaderVariables(pd3dCommandList, pContext);
}

void CTextureToFullScreenShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext)
{
	CPostProcessingShader::Render(pd3dCommandList, pCamera, pContext);
}