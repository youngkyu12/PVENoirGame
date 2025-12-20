#pragma once
#include "Mesh.h"
#include "Camera.h"

class CShader;

struct MATERIAL {
	XMFLOAT4 m_xmf4Ambient;
	XMFLOAT4 m_xmf4Diffuse;
	XMFLOAT4 m_xmf4Specular; //(r,g,b,a=power)
	XMFLOAT4 m_xmf4Emissive;
};

class CMaterial {
public:
	CMaterial() {}
	virtual ~CMaterial() {}

	void ReleaseObjects();

public:
	
	void SetAlbedo(XMFLOAT4& xmf4Albedo) { m_xmf4Albedo = xmf4Albedo; }
	void SetReflection(UINT nReflection) { m_nReflection = nReflection; }
	void SetShader(shared_ptr<CShader> pShader) { m_pShader = pShader; }

public:
	//재질의 기본 색상
	XMFLOAT4 m_xmf4Albedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	//재질의 번호
	UINT m_nReflection = 0;
	//재질을 적용하여 렌더링을 하기 위한 쉐이더
	shared_ptr<CShader> m_pShader;
};

class CGameObject {
public:
	CGameObject() {}
	virtual ~CGameObject() {}

	virtual void ReleaseObjects();
	virtual void ReleaseUploadBuffers();

// Build
public:
	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) {}

// Render
public:
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList) {}

	virtual void Animate(float fTimeElapsed) {}
	virtual void OnPrepareRender() {}
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);

	void MoveStrafe(float fDistance = 1.0f);
	void MoveUp(float fDistance = 1.0f);
	void MoveForward(float fDistance = 1.0f);

	void Rotate(XMFLOAT3* pxmf3Axis, float fAngle);
	virtual void Rotate(float fPitch = 10.0f, float fYaw = 10.0f, float fRoll = 10.0f);

// Get & Set Method
public:
	void SetMesh(shared_ptr<CMesh> pMesh) { m_pMesh = pMesh; }
	void SetShader(shared_ptr<CShader> pShader) {
		if (!m_pMaterial)
			m_pMaterial = make_shared<CMaterial>();

		if (m_pMaterial)
			m_pMaterial->SetShader(pShader);
	}
	void SetMaterial(shared_ptr<CMaterial> pMaterial){ m_pMaterial = pMaterial; }
	void SetMaterial(UINT nReflection) {
		if (!m_pMaterial)
			m_pMaterial = make_shared<CMaterial>();

		m_pMaterial->m_nReflection = nReflection;
	}

	XMFLOAT3 GetPosition() { return(XMFLOAT3(m_xmf4x4World._41, m_xmf4x4World._42, m_xmf4x4World._43)); }
	XMFLOAT3 GetLook() { return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._31, m_xmf4x4World._32, m_xmf4x4World._33))); }
	XMFLOAT3 GetUp() { return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._21, m_xmf4x4World._22, m_xmf4x4World._23))); }
	XMFLOAT3 GetRight() { return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._11, m_xmf4x4World._12, m_xmf4x4World._13))); }

	virtual void SetPosition(float x, float y, float z) { 
		m_xmf4x4World._41 = x; 
		m_xmf4x4World._42 = y;
		m_xmf4x4World._43 = z; 
	}
	virtual void SetPosition(const XMFLOAT3& xmf3Position) { SetPosition(xmf3Position.x, xmf3Position.y, xmf3Position.z); }

	virtual void SetRotationSpeed(float fRotationSpeed) {}
	virtual void SetRotationAxis(XMFLOAT3 xmf3RotationAxis) {}

public:
	XMFLOAT4X4 m_xmf4x4World = Matrix4x4::Identity();
	shared_ptr<CMesh> m_pMesh;
	shared_ptr<CMaterial> m_pMaterial;
};

class CRotatingObject : public CGameObject {
public:
	CRotatingObject() {}
	virtual ~CRotatingObject() {}

// Build
public:
	

// Render
public:
	virtual void Animate(float fTimeElapsed);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);

// Get & Set Method
public:
	virtual void SetRotationSpeed(float fRotationSpeed) { m_fRotationSpeed = fRotationSpeed; }
	virtual void SetRotationAxis(XMFLOAT3 xmf3RotationAxis) { m_xmf3RotationAxis = xmf3RotationAxis; }

private:
	XMFLOAT3 m_xmf3RotationAxis = XMFLOAT3(0.0f, 1.0f, 0.0f);
	float m_fRotationSpeed = 90.0f;
};