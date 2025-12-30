#include "stdafx.h"
#include "Object.h"
#include "Shader.h"

void CGameObject::ReleaseObjects()
{
	if (m_pMesh) m_pMesh->ReleaseObjects();
}

void CGameObject::ReleaseUploadBuffers()
{
	if (m_pMesh) m_pMesh->ReleaseUploadBuffers();
}
