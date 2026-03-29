#include "stdafx.h"
#include "Camera.h"
#include "Object.h"

CCamera::CCamera(CGameObject* owner) : CComponentT(owner)
{
}

CCamera::~CCamera()
{ 
	ReleaseShaderVariables();
}

void CCamera::SetViewport(int xTopLeft, int yTopLeft, int nWidth, int nHeight, float fMinZ, float fMaxZ)
{
	m_d3dViewport.TopLeftX = float(xTopLeft);
	m_d3dViewport.TopLeftY = float(yTopLeft);
	m_d3dViewport.Width = float(nWidth);
	m_d3dViewport.Height = float(nHeight);
	m_d3dViewport.MinDepth = fMinZ;
	m_d3dViewport.MaxDepth = fMaxZ;
}

void CCamera::SetScissorRect(LONG xLeft, LONG yTop, LONG xRight, LONG yBottom)
{
	m_d3dScissorRect.left = xLeft;
	m_d3dScissorRect.top = yTop;
	m_d3dScissorRect.right = xRight;
	m_d3dScissorRect.bottom = yBottom;
}

void CCamera::GenerateProjectionMatrix(float fNearPlaneDistance, float fFarPlaneDistance, float fAspectRatio, float fFOVAngle)
{
	m_xmf4x4Projection = Matrix4x4::PerspectiveFovLH(XMConvertToRadians(fFOVAngle), fAspectRatio, fNearPlaneDistance, fFarPlaneDistance);
//	XMMATRIX xmmtxProjection = XMMatrixPerspectiveFovLH(XMConvertToRadians(fFOVAngle), fAspectRatio, fNearPlaneDistance, fFarPlaneDistance);
//	XMStoreFloat4x4(&m_xmf4x4Projection, xmmtxProjection);
	UpdateBoundingFrustum();
}

void CCamera::GenerateViewMatrix(XMFLOAT3 xmf3Position, XMFLOAT3 xmf3LookAt, XMFLOAT3 xmf3Up)
{
	m_xmf3Position = xmf3Position;
	m_xmf3LookAtWorld = xmf3LookAt;
	m_xmf3Up = xmf3Up;

	GenerateViewMatrix();
}

void CCamera::GenerateViewMatrix()
{
	m_xmf4x4View = Matrix4x4::LookAtLH(m_xmf3Position, m_xmf3LookAtWorld, m_xmf3Up);
}

void CCamera::RegenerateViewMatrix()
{
	m_xmf3Look = Vector3::Normalize(m_xmf3Look);
	m_xmf3Right = Vector3::CrossProduct(m_xmf3Up, m_xmf3Look, true);
	m_xmf3Up = Vector3::CrossProduct(m_xmf3Look, m_xmf3Right, true);

	m_xmf4x4View._11 = m_xmf3Right.x; m_xmf4x4View._12 = m_xmf3Up.x; m_xmf4x4View._13 = m_xmf3Look.x;
	m_xmf4x4View._21 = m_xmf3Right.y; m_xmf4x4View._22 = m_xmf3Up.y; m_xmf4x4View._23 = m_xmf3Look.y;
	m_xmf4x4View._31 = m_xmf3Right.z; m_xmf4x4View._32 = m_xmf3Up.z; m_xmf4x4View._33 = m_xmf3Look.z;
	m_xmf4x4View._41 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Right);
	m_xmf4x4View._42 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Up);
	m_xmf4x4View._43 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Look);

	UpdateBoundingFrustum();
}

void CCamera::CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList)
{
	UINT ncbElementBytes = ((sizeof(VS_CB_CAMERA_INFO) + 255) & ~255); //256의 배수
	m_pd3dcbCamera = ::CreateBufferResource(
		pd3dDevice, 
		pd3dCommandList, 
		NULL, 
		ncbElementBytes, 
		D3D12_HEAP_TYPE_UPLOAD, 
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 
		NULL);

	m_pd3dcbCamera->Map(0, NULL, (void **)&m_pcbMappedCamera);
}

void CCamera::UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList)
{
	XMStoreFloat4x4(&m_pcbMappedCamera->m_xmf4x4View, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4View)));
	XMStoreFloat4x4(&m_pcbMappedCamera->m_xmf4x4Projection, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4Projection)));
	::memcpy(&m_pcbMappedCamera->m_xmf3Position, &m_xmf3Position, sizeof(XMFLOAT3));

	D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = m_pd3dcbCamera->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_CAMERA, d3dGpuVirtualAddress);
}

void CCamera::ReleaseShaderVariables()
{
	if (m_pd3dcbCamera)
	{
		m_pd3dcbCamera->Unmap(0, nullptr);
		m_pd3dcbCamera.Reset();
	}
}

void CCamera::SetViewportsAndScissorRects(ID3D12GraphicsCommandList *pd3dCommandList)
{
	pd3dCommandList->RSSetViewports(1, &m_d3dViewport);
	pd3dCommandList->RSSetScissorRects(1, &m_d3dScissorRect);
}

void CCamera::UpdateBoundingFrustum()
{
	// 1. Projection 기준의 로컬 프러스텀 생성
	BoundingFrustum localFrustum;

	XMMATRIX proj = XMLoadFloat4x4(&m_xmf4x4Projection);
	BoundingFrustum::CreateFromMatrix(localFrustum, proj);

	// 2. 카메라 회전행렬 생성
	XMVECTOR right = XMLoadFloat3(&m_xmf3Right);
	XMVECTOR up = XMLoadFloat3(&m_xmf3Up);
	XMVECTOR look = XMLoadFloat3(&m_xmf3Look);
	XMVECTOR pos = XMLoadFloat3(&m_xmf3Position);

	right = XMVector3Normalize(right);
	up = XMVector3Normalize(up);
	look = XMVector3Normalize(look);

	// DirectXMath의 XMMATRIX는 row-major 생성 형태로 많이 다루니까
	// 축벡터를 행으로 넣어서 회전행렬을 만든다.
	XMMATRIX rot(
		XMVectorSetW(right, 0.0f),
		XMVectorSetW(up, 0.0f),
		XMVectorSetW(look, 0.0f),
		XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)
	);

	// 3. 회전행렬에서 쿼터니언 생성
	XMVECTOR orientation = XMQuaternionRotationMatrix(rot);

	// 4. 로컬 프러스텀을 월드 공간으로 변환
	localFrustum.Transform(m_xmBoundingFrustum, 1.0f, orientation, pos);
}

void CCamera::OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
	CreateShaderVariables(dev, cmd);
}

void CCamera::OnDestroy()
{
	ReleaseShaderVariables();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CThirdPersonCamera

CThirdPersonCamera::CThirdPersonCamera(CGameObject* owner) : CCamera(owner)
{
	m_nMode = THIRD_PERSON_CAMERA;
}

void CThirdPersonCamera::Rotate(float fPitch, float fYaw, float /*fRoll*/)
{
	m_fYaw += fYaw;
	if ( m_fYaw >= 360.0f || m_fYaw <= -360.0f )
		m_fYaw = fmodf(m_fYaw, 360.0f);

	m_fPitch += fPitch;

	if ( m_fPitch < -30.0f ) m_fPitch = -30.0f;
	if ( m_fPitch > 60.0f )  m_fPitch = 60.0f;
}

void CThirdPersonCamera::Update(XMFLOAT3& xmf3LookAt, float fTimeElapsed)
{
	if ( !m_pTarget ) return;

	m_xmf3LookAtWorld = xmf3LookAt;

	const XMMATRIX rotM = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(m_fPitch),
		XMConvertToRadians(m_fYaw),
		0.0f
	);

	XMFLOAT3 offsetW{};
	XMStoreFloat3(
		&offsetW,
		XMVector3TransformCoord(XMLoadFloat3(&m_xmf3Offset), rotM)
	);

	const XMFLOAT3 desiredPos = Vector3::Add(xmf3LookAt, offsetW);

	if ( m_fTimeLag > 0.0f )
	{
		XMFLOAT3 dir = Vector3::Subtract(desiredPos, m_xmf3Position);
		const float len = Vector3::Length(dir);

		if ( len > 1e-6f )
		{
			dir = Vector3::Normalize(dir);

			float moveDist = len * ( fTimeElapsed / m_fTimeLag );
			if ( moveDist > len ) moveDist = len;

			m_xmf3Position = Vector3::Add(m_xmf3Position, dir, moveDist);
		}
		else
		{
			m_xmf3Position = desiredPos;
		}
	}
	else
	{
		m_xmf3Position = desiredPos;
	}

	XMStoreFloat3(
		&m_xmf3Right,
		XMVector3Normalize(
			XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rotM)
		)
	);

	XMStoreFloat3(
		&m_xmf3Up,
		XMVector3Normalize(
			XMVector3TransformNormal(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotM)
		)
	);

	XMStoreFloat3(
		&m_xmf3Look,
		XMVector3Normalize(
			XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotM)
		)
	);
}

void CThirdPersonCamera::SetLookAt(XMFLOAT3& vLookAt)
{
	m_xmf3LookAtWorld = vLookAt;
}
