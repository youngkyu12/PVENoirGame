#pragma once

#define DIR_FORWARD				0x01
#define DIR_BACKWARD			0x02
#define DIR_LEFT				0x04
#define DIR_RIGHT				0x08
#define DIR_UP					0x10
#define DIR_DOWN				0x20

#include "Object.h"
#include "Camera.h"
#include "AnimatorData.h"

struct CB_PLAYER_INFO
{
	XMFLOAT4X4 m_xmf4x4World;
};

class CPlayer : public CGameObject
{
protected:
	XMFLOAT3					m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3     				m_xmf3Gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float           			m_fYaw = 0.0f;

	float           			m_fMaxVelocityXZ = 0.0f;
	float           			m_fMaxVelocityY = 0.0f;
	float           			m_fFriction = 0.0f;

	LPVOID						m_pPlayerUpdatedContext = nullptr;
	LPVOID						m_pCameraUpdatedContext = nullptr;

	unique_ptr<CCamera>			m_pCamera;

public:
	CPlayer(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, void *pContext=NULL, int nMeshes = 1);
	virtual ~CPlayer();

	XMFLOAT3 GetPosition() { return (m_pTransform ? m_pTransform->position : XMFLOAT3(0, 0, 0)); }
	XMFLOAT3 GetLookVector() { return (m_pTransform ? m_pTransform->GetLook() : XMFLOAT3(0, 0, 1)); }
	XMFLOAT3 GetUpVector() { return (m_pTransform ? m_pTransform->GetUp() : XMFLOAT3(0, 1, 0)); }
	XMFLOAT3 GetRightVector() { return (m_pTransform ? m_pTransform->GetRight() : XMFLOAT3(1, 0, 0)); }

	void SetFriction(float fFriction) { m_fFriction = fFriction; }
	void SetGravity(const XMFLOAT3& xmf3Gravity) { m_xmf3Gravity = xmf3Gravity; }
	void SetMaxVelocityXZ(float fMaxVelocity) { m_fMaxVelocityXZ = fMaxVelocity; }
	void SetMaxVelocityY(float fMaxVelocity) { m_fMaxVelocityY = fMaxVelocity; }
	void SetVelocity(const XMFLOAT3& xmf3Velocity) { m_xmf3Velocity = xmf3Velocity; }
	void SetPosition(const XMFLOAT3& xmf3Position)
	{
		XMFLOAT3 cur = GetPosition();
		Move(XMFLOAT3(xmf3Position.x - cur.x, xmf3Position.y - cur.y, xmf3Position.z - cur.z), false);
	}

	const XMFLOAT3& GetVelocity() const { return(m_xmf3Velocity); }
	float GetYaw() const { return(m_fYaw); }

	CCamera *GetCamera() { return(m_pCamera.get()); }
	void SetCamera(unique_ptr<CCamera> pCamera) { m_pCamera = move(pCamera); }

	enum class EVerticalMoveSpace : uint8_t
	{
		WorldUp,
		LocalUp
	};
	void Move(DWORD dwDirection, float fDistance, bool bVelocity = false,
		EVerticalMoveSpace upSpace = EVerticalMoveSpace::WorldUp); 
	void Move(const XMFLOAT3& xmf3Shift, bool bVelocity = false);
	void Rotate(float x, float y, float z);

	void Update(float fTimeElapsed);

	virtual void OnPlayerUpdateCallback(float fTimeElapsed) { }
	void SetPlayerUpdatedContext(LPVOID pContext) { m_pPlayerUpdatedContext = pContext; }

	virtual void OnCameraUpdateCallback(float fTimeElapsed) { }
	void SetCameraUpdatedContext(LPVOID pContext) { m_pCameraUpdatedContext = pContext; }

	virtual void CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList);
	virtual void ReleaseShaderVariables();
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList);

	unique_ptr<CCamera> OnChangeCamera(DWORD nNewCameraMode, DWORD nCurrentCameraMode);

	virtual CCamera *ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed) { return(NULL); }
	virtual void OnPrepareRender(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera);
	virtual void SetRootParameter(ID3D12GraphicsCommandList *pd3dCommandList);
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera = NULL);

protected:
	ComPtr<ID3D12Resource>			m_pd3dcbPlayer;
	CB_PLAYER_INFO					*m_pcbMappedPlayer = nullptr;

};

class CFighterPlayer : public CPlayer
{
public:
	CFighterPlayer(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, void *pContext=NULL, int nMeshes=1);
	virtual ~CFighterPlayer();

	virtual CCamera *ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed);
	virtual void OnPrepareRender(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera);
};


