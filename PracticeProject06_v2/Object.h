#pragma once
#include "Mesh.h"

class CShader;

class CGameObject
{
public:
	CGameObject();
	virtual ~CGameObject();

// Build
public:
	void SetMesh(shared_ptr<CMesh> pMesh);
	void SetShader(shared_ptr<CShader> pShader);

// Render
public:
	void Animate(float fTimeElapsed);
	void OnPrepareRender();
	void Render(ID3D12GraphicsCommandList* pd3dCommandList);

protected:
	XMFLOAT4X4 m_xmf4x4World;
	shared_ptr<CMesh> m_pMesh;
	shared_ptr<CShader> m_pShader;
};