#pragma once
#include "Mesh.h"
#include "Camera.h"

class CShader;

class CGameObject {
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
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);

	void Rotate(XMFLOAT3* pxmf3Axis, float fAngle);

protected:
	XMFLOAT4X4 m_xmf4x4World;
	shared_ptr<CMesh> m_pMesh;
	shared_ptr<CShader> m_pShader;
};

class CRotatingObject : public CGameObject {
public:
	CRotatingObject();
	virtual ~CRotatingObject();

// Build
public:
	void SetRotationSpeed(float fRotationSpeed) { m_fRotationSpeed = fRotationSpeed; }
	void SetRotationAxis(XMFLOAT3 xmf3RotationAxis) { m_xmf3RotationAxis = xmf3RotationAxis; }

// Render
public:
	virtual void Animate(float fTimeElapsed);

private:
	XMFLOAT3 m_xmf3RotationAxis = XMFLOAT3(0.0f, 1.0f, 0.0f);
	float m_fRotationSpeed = 90.0f;
};