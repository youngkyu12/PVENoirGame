#include "stdafx.h"
#include "Object.h"
#include "Shader.h"

void CRotatingObject::ReleaseObjects()
{
	if (m_pMesh) m_pMesh->ReleaseObjects();
}

void CRotatingObject::ReleaseUploadBuffers()
{
	if (m_pMesh) m_pMesh->ReleaseUploadBuffers();
}
