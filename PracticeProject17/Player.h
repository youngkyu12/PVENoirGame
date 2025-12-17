#pragma once

#include "Object.h"
#include "Camera.h"

class CPlayer : public CGameObject {
public:
	CPlayer() {}
	virtual ~CPlayer() {}
	
	unique_ptr<CCamera> OnChangeCamera(DWORD nNewCameraMode, DWORD nCurrentCameraMode);
	virtual void ReleaseObjects();

	// Build
public:
	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

// Render
public:
	//플레이어를 이동하는 함수이다.
	void Move(DWORD nDirection, float fDistance, bool bVelocity = false);
	void Move(const XMFLOAT3& xmf3Shift, bool bVelocity = false);

	//플레이어를 회전하는 함수이다.
	virtual void Rotate(float fPitch, float fYaw, float fRoll);

	//플레이어의 위치와 회전 정보를 경과 시간에 따라 갱신하는 함수이다.
	void Update(float fTimeElapsed);

	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);

	virtual CCamera* ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed) { return(NULL); }

	virtual void OnPrepareRender();
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera = NULL);

// Get & Set Method
public:
	XMFLOAT3 GetPosition() { return(m_xmf3Position); }
	XMFLOAT3 GetLookVector() { return(m_xmf3Look); }
	XMFLOAT3 GetUpVector() { return(m_xmf3Up); }
	XMFLOAT3 GetRightVector() { return(m_xmf3Right); }

	void SetFriction(float fFriction) { m_fFriction = fFriction; }
	void SetGravity(const XMFLOAT3& xmf3Gravity) { m_xmf3Gravity = xmf3Gravity; }
	void SetMaxVelocityXZ(float fMaxVelocity) { m_fMaxVelocityXZ = fMaxVelocity; }
	void SetMaxVelocityY(float fMaxVelocity) { m_fMaxVelocityY = fMaxVelocity; }
	void SetVelocity(const XMFLOAT3& xmf3Velocity) { m_xmf3Velocity = xmf3Velocity; }

	virtual void SetPosition(const XMFLOAT3& xmf3Position) {
		Move(XMFLOAT3(xmf3Position.x - m_xmf3Position.x, xmf3Position.y - m_xmf3Position.y, xmf3Position.z - m_xmf3Position.z), false);
	}

	XMFLOAT3& GetVelocity() { return(m_xmf3Velocity); }
	float GetYaw() { return(m_fYaw); }
	float GetPitch() { return(m_fPitch); }
	float GetRoll() { return(m_fRoll); }

	CCamera* GetCamera() { return m_pCamera.get(); }
	void SetCamera(unique_ptr<CCamera> pCamera) { m_pCamera = move(pCamera); }

	//플레이어의 위치가 바뀔 때마다 호출되는 함수와 그 함수에서 사용하는 정보를 설정하는 함수이다.
	virtual void OnPlayerUpdateCallback(float fTimeElapsed) {}
	void SetPlayerUpdatedContext(LPVOID pContext) { m_pPlayerUpdatedContext = pContext; }

	//카메라의 위치가 바뀔 때마다 호출되는 함수와 그 함수에서 사용하는 정보를 설정하는 함수이다.
	virtual void OnCameraUpdateCallback(float fTimeElapsed) {}
	void SetCameraUpdatedContext(LPVOID pContext) { m_pCameraUpdatedContext = pContext; }

protected:
	XMFLOAT3	m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3	m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMFLOAT3	m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMFLOAT3	m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);

	float	m_fPitch = 0.0f;
	float	m_fYaw = 0.0f;
	float	m_fRoll = 0.0f;

	XMFLOAT3	m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3	m_xmf3Gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float	m_fMaxVelocityXZ = 0.0f;

	float	m_fMaxVelocityY = 0.0f;
	float	m_fFriction = 0.0f;

	LPVOID	m_pPlayerUpdatedContext = NULL;
	LPVOID	m_pCameraUpdatedContext = NULL;

	unique_ptr<CCamera> m_pCamera;
};

class CAirplanePlayer : public CPlayer {
public:
	CAirplanePlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature);
	virtual ~CAirplanePlayer() {}

	virtual CCamera* ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed);
	virtual void OnPrepareRender();
};