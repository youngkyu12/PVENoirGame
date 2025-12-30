//------------------------------------------------------- ----------------------
// File: Object.h
//-----------------------------------------------------------------------------

#pragma once

#include "Mesh.h"
#include "Camera.h"

#define DIR_FORWARD					0x01
#define DIR_BACKWARD				0x02
#define DIR_LEFT					0x04
#define DIR_RIGHT					0x08
#define DIR_UP						0x10
#define DIR_DOWN					0x20

class CShader;
class CMaterial;

struct CB_GAMEOBJECT_INFO
{
	XMFLOAT4X4						m_xmf4x4World;
	UINT							m_nObjectID;
	UINT							m_nMaterialID;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CGameObject
{
public:
	CGameObject(int nMeshes=1);
	virtual ~CGameObject();

	virtual void ReleaseShaderVariables();
	virtual void ReleaseUploadBuffers();

// Build
public:
	virtual void CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList);
	virtual void BuildMaterials(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) {}

// Render
public:
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);

	virtual void Animate(float fTimeElapsed);

	virtual void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera = nullptr);

	void MoveStrafe(float fDistance = 1.0f);
	void MoveUp(float fDistance = 1.0f);
	void MoveForward(float fDistance = 1.0f);

	void Rotate(float fPitch = 10.0f, float fYaw = 10.0f, float fRoll = 10.0f);
	void Rotate(XMFLOAT3 *pxmf3Axis, float fAngle);

// Get & Set Method
public:
	void SetMesh(int nIndex, shared_ptr<CMesh> pMesh);
	void SetShader(shared_ptr<CShader> pShader);
	void SetMaterial(shared_ptr<CMaterial> pMaterial);

	void SetCbvGPUDescriptorHandle(D3D12_GPU_DESCRIPTOR_HANDLE d3dCbvGPUDescriptorHandle) { m_d3dCbvGPUDescriptorHandle = d3dCbvGPUDescriptorHandle; }
	void SetCbvGPUDescriptorHandlePtr(UINT64 nCbvGPUDescriptorHandlePtr) { m_d3dCbvGPUDescriptorHandle.ptr = nCbvGPUDescriptorHandlePtr; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetCbvGPUDescriptorHandle() { return(m_d3dCbvGPUDescriptorHandle); }

	virtual void SetRootParameter(ID3D12GraphicsCommandList* pd3dCommandList) {
		pd3dCommandList->SetGraphicsRootDescriptorTable(ROOT_PARAMETER_OBJECT, m_d3dCbvGPUDescriptorHandle);
	}

	XMFLOAT3 GetPosition() const { return(XMFLOAT3(m_xmf4x4World._41, m_xmf4x4World._42, m_xmf4x4World._43)); }
	XMFLOAT3 GetLook() const { return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._31, m_xmf4x4World._32, m_xmf4x4World._33))); }
	XMFLOAT3 GetUp() const { return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._21, m_xmf4x4World._22, m_xmf4x4World._23))); }
	XMFLOAT3 GetRight() const { return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._11, m_xmf4x4World._12, m_xmf4x4World._13))); }

	void SetPosition(float x, float y, float z) {
		m_xmf4x4World._41 = x;
		m_xmf4x4World._42 = y;
		m_xmf4x4World._43 = z;
	}
	void SetPosition(XMFLOAT3 xmf3Position) {
		SetPosition(xmf3Position.x, xmf3Position.y, xmf3Position.z);
	}


public:
	XMFLOAT4X4						m_xmf4x4World = Matrix4x4::Identity();

	vector<shared_ptr<CMesh>>		m_ppMeshes;
	int								m_nMeshes = 0;

	shared_ptr<CMaterial>			m_pMaterial;

	D3D12_GPU_DESCRIPTOR_HANDLE		m_d3dCbvGPUDescriptorHandle = { 0 };

protected:
	ComPtr<ID3D12Resource>			m_pd3dcbGameObject;
	CB_GAMEOBJECT_INFO* m_pcbMappedGameObject = nullptr;
};

class CRotatingObject : public CGameObject
{
public:
	CRotatingObject(int nMeshes=1);
	virtual ~CRotatingObject();

private:
	XMFLOAT3					m_xmf3RotationAxis = XMFLOAT3(0.0f, 1.0f, 0.0f);
	float						m_fRotationSpeed = 15.0f;

public:
	void SetRotationSpeed(float fRotationSpeed) { m_fRotationSpeed = fRotationSpeed; }
	void SetRotationAxis(XMFLOAT3 xmf3RotationAxis) { m_xmf3RotationAxis = xmf3RotationAxis; }

	virtual void Animate(float fTimeElapsed);
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera = NULL);
};

class CRevolvingObject : public CGameObject
{
public:
	CRevolvingObject(int nMeshes=1);
	virtual ~CRevolvingObject();

private:
	XMFLOAT3					m_xmf3RevolutionAxis = XMFLOAT3(1.0f, 0.0f, 0.0f);
	float						m_fRevolutionSpeed = 0.0f;

public:
	void SetRevolutionSpeed(float fRevolutionSpeed) { m_fRevolutionSpeed = fRevolutionSpeed; }
	void SetRevolutionAxis(XMFLOAT3 xmf3RevolutionAxis) { m_xmf3RevolutionAxis = xmf3RevolutionAxis; }

	virtual void Animate(float fTimeElapsed);
};

