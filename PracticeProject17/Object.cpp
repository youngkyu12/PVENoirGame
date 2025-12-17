#include "stdafx.h"
#include "Object.h"
#include "Shader.h"

void CGameObject::ReleaseObjects()
{
	if (m_pMaterial) m_pMaterial->ReleaseObjects();
	if (m_pMesh) m_pMesh->ReleaseObjects();
}

void CGameObject::ReleaseUploadBuffers()
{
	if (m_pMesh) m_pMesh->ReleaseUploadBuffers();
}

void CMaterial::ReleaseObjects()
{
	if (m_pShader)
	{
		m_pShader->ReleaseShaderVariables();
		m_pShader->ReleaseObjects();
	}
}
