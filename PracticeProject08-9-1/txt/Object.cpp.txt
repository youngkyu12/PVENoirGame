//-----------------------------------------------------------------------------
// File: CGameObject.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Object.h"
#include "Shader.h"
#include "Texture.h"
#include "Material.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CGameObject::CGameObject(int nMeshes)
{
	m_nMeshes = nMeshes;
	if (m_nMeshes > 0)
	{
		m_ppMeshes.resize(m_nMeshes);
	}
}

CGameObject::~CGameObject()
{
	ReleaseShaderVariables();

	if (!m_ppMeshes.empty())
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i])
				m_ppMeshes[i].reset();
		}
	}
}

void CGameObject::ReleaseShaderVariables()
{
	if (m_pd3dcbGameObject)
	{
		m_pd3dcbGameObject->Unmap(0, nullptr);
		m_pd3dcbGameObject.Reset();
	}

	if (m_pMaterial)
		m_pMaterial->ReleaseShaderVariables();
}

void CGameObject::ReleaseUploadBuffers()
{
	if (!m_ppMeshes.empty())
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i])m_ppMeshes[i]->ReleaseUploadBuffers();
		}
	}

	if (m_pMaterial)m_pMaterial->ReleaseUploadBuffers();
}

void CGameObject::SetMesh(int nIndex, shared_ptr<CMesh> pMesh)
{
	if (!m_ppMeshes.empty())
	{
		if (m_ppMeshes[nIndex])
			m_ppMeshes[nIndex].reset();
		m_ppMeshes[nIndex] = pMesh;
	}
}

void CGameObject::SetShader(shared_ptr<CShader> pShader)
{
	if (!m_pMaterial)
	{
		shared_ptr<CMaterial> pMaterial = make_shared<CMaterial>();
		SetMaterial(pMaterial);
	}
	if (m_pMaterial)
		m_pMaterial->SetShader(pShader);
}

void CGameObject::SetMaterial(shared_ptr<CMaterial> pMaterial)
{
	if (m_pMaterial)
		m_pMaterial.reset();
	m_pMaterial = pMaterial;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
CRotatingObject::CRotatingObject(int nMeshes)
{
}

CRotatingObject::~CRotatingObject()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
CRevolvingObject::CRevolvingObject(int nMeshes)
{
}

CRevolvingObject::~CRevolvingObject()
{
}


