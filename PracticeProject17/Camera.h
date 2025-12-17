#pragma once

struct VS_CB_CAMERA_INFO {
	XMFLOAT4X4 m_xmf4x4View;
	XMFLOAT4X4 m_xmf4x4Projection;
	XMFLOAT3   m_xmf3Position;
};

class CPlayer;

class CCamera {
public:
	CCamera() {}
	CCamera(CCamera* pCamera);
	virtual ~CCamera() {}

	virtual void ReleaseShaderVariables();

// Build
public:
	void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

// Render
public:
	virtual void SetViewportsAndScissorRects(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);

	virtual void Move(const XMFLOAT3& xmf3Shift) {
		m_xmf3Position.x += xmf3Shift.x;
		m_xmf3Position.y += xmf3Shift.y;
		m_xmf3Position.z += xmf3Shift.z;
	}
	virtual void Rotate(float fPitch = 0.0f, float fYaw = 0.0f, float fRoll = 0.0f) {}
	virtual void Update(XMFLOAT3& xmf3LookAt, float fTimeElapsed) {}
	virtual void SetLookAt(XMFLOAT3& xmf3LookAt) {}

// Get & Set Method
public:
	void GenerateViewMatrix() {
		m_xmf4x4View = Matrix4x4::LookAtLH(m_xmf3Position, m_xmf3LookAtWorld, m_xmf3Up);
	}

	void GenerateViewMatrix(XMFLOAT3 xmf3Position, XMFLOAT3 xmf3LookAt, XMFLOAT3 xmf3Up) {
		m_xmf4x4View = Matrix4x4::LookAtLH(xmf3Position, xmf3LookAt, xmf3Up);
	}

	void GenerateProjectionMatrix(float fNearPlaneDistance, float fFarPlaneDistance, float fAspectRatio, float fFOVAngle) {
		m_xmf4x4Projection = Matrix4x4::PerspectiveFovLH(XMConvertToRadians(fFOVAngle), fAspectRatio, fNearPlaneDistance, fFarPlaneDistance);
	}

	void RegenerateViewMatrix() {
		m_xmf3Look = Vector3::Normalize(m_xmf3Look);
		m_xmf3Right = Vector3::CrossProduct(m_xmf3Up, m_xmf3Look, true);
		m_xmf3Up = Vector3::CrossProduct(m_xmf3Look, m_xmf3Right, true);

		m_xmf4x4View._11 = m_xmf3Right.x; m_xmf4x4View._12 = m_xmf3Up.x; m_xmf4x4View._13 = m_xmf3Look.x;
		m_xmf4x4View._21 = m_xmf3Right.y; m_xmf4x4View._22 = m_xmf3Up.y; m_xmf4x4View._23 = m_xmf3Look.y;
		m_xmf4x4View._31 = m_xmf3Right.z; m_xmf4x4View._32 = m_xmf3Up.z; m_xmf4x4View._33 = m_xmf3Look.z;
		m_xmf4x4View._41 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Right);
		m_xmf4x4View._42 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Up);
		m_xmf4x4View._43 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Look);
	}

	void SetViewport(int xTopLeft, int yTopLeft, int nWidth, int nHeight, float fMinZ = 0.0f, float fMaxZ = 1.0f) {
		m_d3dViewport.TopLeftX = float(xTopLeft);
		m_d3dViewport.TopLeftY = float(yTopLeft);
		m_d3dViewport.Width = float(nWidth);
		m_d3dViewport.Height = float(nHeight);
		m_d3dViewport.MinDepth = fMinZ;
		m_d3dViewport.MaxDepth = fMaxZ;
	}

	void SetScissorRect(LONG xLeft, LONG yTop, LONG xRight, LONG yBottom) {
		m_d3dScissorRect.left = xLeft;
		m_d3dScissorRect.top = yTop;
		m_d3dScissorRect.right = xRight;
		m_d3dScissorRect.bottom = yBottom;
	}

	void SetPlayer(CPlayer* pPlayer) { m_pPlayer = pPlayer; }
	CPlayer* GetPlayer() { return(m_pPlayer); }

	void SetMode(DWORD nMode) { m_nMode = nMode; }
	DWORD GetMode() { return(m_nMode); }

	void SetPosition(XMFLOAT3 xmf3Position) { m_xmf3Position = xmf3Position; }
	XMFLOAT3& GetPosition() { return(m_xmf3Position); }

	void SetLookAtPosition(XMFLOAT3 xmf3LookAtWorld) {
		m_xmf3LookAtWorld = xmf3LookAtWorld;
	}

	XMFLOAT3& GetLookAtPosition() { return(m_xmf3LookAtWorld); }
	XMFLOAT3& GetRightVector() { return(m_xmf3Right); }
	XMFLOAT3& GetUpVector() { return(m_xmf3Up); }
	XMFLOAT3& GetLookVector() { return(m_xmf3Look); }

	float& GetPitch() { return(m_fPitch); }
	float& GetRoll() { return(m_fRoll); }
	float& GetYaw() { return(m_fYaw); }

	void SetOffset(XMFLOAT3 xmf3Offset) { m_xmf3Offset = xmf3Offset; }
	XMFLOAT3& GetOffset() { return(m_xmf3Offset); }

	void SetTimeLag(float fTimeLag) { m_fTimeLag = fTimeLag; }
	float GetTimeLag() { return(m_fTimeLag); }

	XMFLOAT4X4 GetViewMatrix() { return(m_xmf4x4View); }
	XMFLOAT4X4 GetProjectionMatrix() { return(m_xmf4x4Projection); }
	D3D12_VIEWPORT GetViewport() { return(m_d3dViewport); }
	D3D12_RECT GetScissorRect() { return(m_d3dScissorRect); }


protected:
	XMFLOAT3		m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);

	XMFLOAT3		m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMFLOAT3		m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMFLOAT3		m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);

	float		m_fPitch = 0.0f;
	float		m_fRoll = 0.0f;
	float		m_fYaw = 0.0f;

	DWORD		m_nMode = 0x00;
	XMFLOAT3		m_xmf3LookAtWorld = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3		m_xmf3Offset = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float	m_fTimeLag = 0.0f;


	XMFLOAT4X4 m_xmf4x4View = Matrix4x4::Identity();
	XMFLOAT4X4 m_xmf4x4Projection = Matrix4x4::Identity();

	D3D12_VIEWPORT m_d3dViewport = { 0, 0, FRAME_BUFFER_WIDTH , FRAME_BUFFER_HEIGHT, 0.0f, 1.0f };
	D3D12_RECT m_d3dScissorRect = { 0, 0, FRAME_BUFFER_WIDTH , FRAME_BUFFER_HEIGHT };

	CPlayer* m_pPlayer = nullptr;

	ComPtr<ID3D12Resource> m_pd3dcbCamera;
	VS_CB_CAMERA_INFO* m_pcbMappedCamera = nullptr;
};

class CSpaceShipCamera : public CCamera {
public:
	CSpaceShipCamera(CCamera* pCamera);
	virtual ~CSpaceShipCamera() {}

	virtual void Rotate(float fPitch = 0.0f, float fYaw = 0.0f, float fRoll = 0.0f);
};

class CFirstPersonCamera : public CCamera {
public:
	CFirstPersonCamera(CCamera* pCamera);
	virtual ~CFirstPersonCamera() {}

	virtual void Rotate(float fPitch = 0.0f, float fYaw = 0.0f, float fRoll = 0.0f);
};

class CThirdPersonCamera : public CCamera {
public:
	CThirdPersonCamera(CCamera* pCamera);
	virtual ~CThirdPersonCamera() {}

	virtual void Update(XMFLOAT3& xmf3LookAt, float fTimeElapsed);
	virtual void SetLookAt(XMFLOAT3& vLookAt);
};