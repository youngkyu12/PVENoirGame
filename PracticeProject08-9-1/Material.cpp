//-----------------------------------------------------------------------------
// File: CTexture.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Material.h"

CMaterial::CMaterial()
{
}

CMaterial::~CMaterial()
{
}

void CMaterial::SetTexture(shared_ptr<CTexture> pTexture)
{
	m_pTexture = pTexture;
	m_nDiffuseSrvIndex = (pTexture ? pTexture->GetBaseSrvIndex() : UINT_MAX);
}

void CMaterial::SetNormalTexture(shared_ptr<CTexture> pTexture)
{
	m_pNormalTexture = pTexture;
	m_nNormalSrvIndex = (pTexture ? pTexture->GetBaseSrvIndex() : UINT_MAX);

}
void CMaterial::SetShader(shared_ptr<CShader> pShader)
{
	if (m_pShader)
		m_pShader.reset();
	m_pShader = pShader;
}

void CMaterial::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	/*
	// 글로벌 SRV 인덱스 기반이면 여기서 바인딩할 게 없다.
	if (!NeedsLegacyBinding()) return;

	m_pTexture->UpdateShaderVariables(pd3dCommandList);
	*/
}

void CMaterial::ReleaseShaderVariables()
{
	if (m_pShader) m_pShader->ReleaseShaderVariables();

	if (m_pTexture)       m_pTexture->ReleaseShaderVariables();
	if (m_pNormalTexture) m_pNormalTexture->ReleaseShaderVariables();
}

void CMaterial::ReleaseUploadBuffers()
{
	if (m_pTexture)       m_pTexture->ReleaseUploadBuffers();
	if (m_pNormalTexture) m_pNormalTexture->ReleaseUploadBuffers();
}

bool CMaterial::NeedsLegacyBinding() const
{
	// baseSrvIndex가 없으면(=UINT_MAX) 레거시 바인딩 경로를 쓰는 텍스처
	return (m_pTexture && m_pTexture->GetBaseSrvIndex() == UINT_MAX);
}
