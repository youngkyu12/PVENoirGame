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
	virtual void SetMesh(shared_ptr<CMesh> pMesh);
	virtual void SetShader(shared_ptr<CShader> pShader);

// Render
public:
	virtual void Animate(float fTimeElapsed);
	virtual void OnPrepareRender();
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList);

protected:
	XMFLOAT4X4 m_xmf4x4World;
	shared_ptr<CMesh> m_pMesh = nullptr;
	shared_ptr<CShader> m_pShader = nullptr;
};