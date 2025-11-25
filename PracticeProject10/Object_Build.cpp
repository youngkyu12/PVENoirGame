#include "stdafx.h"
#include "Object.h"
#include "Shader.h"

void CGameObject::SetMesh(shared_ptr<CMesh> pMesh)
{
	m_pMesh = pMesh;
}

void CGameObject::SetShader(shared_ptr<CShader> pShader)
{
	m_pShader = pShader;
}