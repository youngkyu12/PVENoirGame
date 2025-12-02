#include "stdafx.h"
#include "Object.h"
#include "Shader.h"

void CRotatingObject::SetMesh(shared_ptr<CMesh> pMesh)
{
	m_pMesh = pMesh;
}