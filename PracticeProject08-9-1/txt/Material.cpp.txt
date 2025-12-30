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
	if (m_pTexture)
		m_pTexture.reset();
	m_pTexture = pTexture;
}

void CMaterial::SetShader(shared_ptr<CShader> pShader)
{
	if (m_pShader)
		m_pShader.reset();
	m_pShader = pShader;
}

void CMaterial::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (m_pTexture)
		m_pTexture->UpdateShaderVariables(pd3dCommandList);
}

void CMaterial::ReleaseShaderVariables()
{
	if (m_pShader)
		m_pShader->ReleaseShaderVariables();

	if (m_pTexture)
		m_pTexture->ReleaseShaderVariables();
}

void CMaterial::ReleaseUploadBuffers()
{
	if (m_pTexture)
		m_pTexture->ReleaseUploadBuffers();
}