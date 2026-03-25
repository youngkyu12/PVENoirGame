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


void CThirdPersonCamera::Update(XMFLOAT3& xmf3LookAt, float fTimeElapsed)
{
	if (!m_pTarget) return;

	// 타겟의 월드행렬에서 basis 추출 (row = right/up/look, _41~_43 = pos 라는 기존 스타일 전제)
	const XMFLOAT4X4& W = m_pTarget->GetWorldMatrix();
	XMFLOAT3 right(W._11, W._12, W._13);
	XMFLOAT3 up(W._21, W._22, W._23);
	XMFLOAT3 look(W._31, W._32, W._33);

	XMFLOAT4X4 R = Matrix4x4::Identity();
	R._11 = right.x; R._21 = up.x;   R._31 = look.x;
	R._12 = right.y; R._22 = up.y;   R._32 = look.y;
	R._13 = right.z; R._23 = up.z;   R._33 = look.z;

	XMFLOAT3 offsetW = Vector3::TransformCoord(m_xmf3Offset, R);

	XMFLOAT3 targetPos = m_pTarget->GetPosition();
	XMFLOAT3 desiredPos = Vector3::Add(targetPos, offsetW);

	XMFLOAT3 dir = Vector3::Subtract(desiredPos, m_xmf3Position);
	float len = Vector3::Length(dir);
	dir = Vector3::Normalize(dir);

	float lagScale = (m_fTimeLag) ? fTimeElapsed * (1.0f / m_fTimeLag) : 1.0f;
	float dist = len * lagScale;
	if (dist > len) dist = len;
	if (len < 0.01f) dist = len;

	if (dist > 0.0f)
	{
		m_xmf3Position = Vector3::Add(m_xmf3Position, dir, dist);
		SetLookAt(xmf3LookAt);
	}
}

void CThirdPersonCamera::SetLookAt(XMFLOAT3& xmf3LookAt)
{
	XMFLOAT3 at = xmf3LookAt;
	at.y = m_xmf3Position.y;                 // 수평 시선: target 높이를 카메라 높이로

	XMFLOAT3 up(0.0f, 1.0f, 0.0f);           // (선택) 월드 업 고정까지 같이 하면 더 안정적

	XMFLOAT4X4 mtxLookAt = Matrix4x4::LookAtLH(m_xmf3Position, at, up);
	m_xmf3Right = XMFLOAT3(mtxLookAt._11, mtxLookAt._21, mtxLookAt._31);
	m_xmf3Up = XMFLOAT3(mtxLookAt._12, mtxLookAt._22, mtxLookAt._32);
	m_xmf3Look = XMFLOAT3(mtxLookAt._13, mtxLookAt._23, mtxLookAt._33);

	m_xmf3LookAtWorld = at;                  // (선택) 저장
}
