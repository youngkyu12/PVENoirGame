//-----------------------------------------------------------------------------
// File: CGameObject.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Object.h"
#include "Shader.h"
#include "GameFramework.h"

inline float RandF(float fMin, float fMax)
{
	return(fMin + ((float)rand() / (float)RAND_MAX) * (fMax - fMin));
}

XMVECTOR RandomUnitVectorOnSphere()
{
	XMVECTOR xmvOne = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	XMVECTOR xmvZero = XMVectorZero();

	while (true)
	{
		XMVECTOR v = XMVectorSet(RandF(-1.0f, 1.0f), RandF(-1.0f, 1.0f), RandF(-1.0f, 1.0f), 0.0f);
		if (!XMVector3Greater(XMVector3LengthSq(v), xmvOne)) return(XMVector3Normalize(v));
	}
}

CGameObject::CGameObject(int nMeshes)
{
	m_xmf4x4World = Matrix4x4::Identity();
	m_nMeshes = nMeshes;
	m_ppMeshes = NULL;
	if (m_nMeshes > 0)
	{
		m_ppMeshes = new CMesh * [m_nMeshes];
		for (int i = 0; i < m_nMeshes; i++) m_ppMeshes[i] = NULL;
	}
}

CGameObject::~CGameObject()
{
	if (m_ppMeshes)
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i]) m_ppMeshes[i]->Release();
			m_ppMeshes[i] = NULL;
		}
		delete[] m_ppMeshes;
	}
	if (m_pShader)
	{
		m_pShader->ReleaseShaderVariables();
		m_pShader->Release();
	}
}

void CGameObject::SetMesh(int nIndex, CMesh* pMesh)
{
	if (m_ppMeshes && nIndex >= 0 && nIndex < m_nMeshes) {
		if (m_ppMeshes[nIndex]) m_ppMeshes[nIndex]->Release();
		m_ppMeshes[nIndex] = pMesh;
		if (pMesh) pMesh->AddRef();
	}
}

void CGameObject::SetShader(CShader *pShader)
{
	if (m_pShader) m_pShader->Release();
	m_pShader = pShader;
	if (m_pShader) m_pShader->AddRef();
}

void CGameObject::Animate(float fTimeElapsed)
{
	if (m_pAnimator)
	{
		m_pAnimator->Update(fTimeElapsed);
		auto boneMats = m_pAnimator->GetSkinMatrices();

		D3D12_RANGE range{ 0, 0 };
		void* pData = nullptr;
		m_pd3dBoneCB->Map(0, &range, &pData);
		memcpy(pData, boneMats.data(), sizeof(XMFLOAT4X4) * boneMats.size());
		m_pd3dBoneCB->Unmap(0, nullptr);
	}
}

void CGameObject::CreateShaderVariables(ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList)
{
#ifdef ENABLE_ANIM_SKINNING
	if (m_pAnimator)
	{
		const UINT boneCount = static_cast<UINT>(m_pAnimator->GetBoneCount());
		if (boneCount > 0)
		{
			m_nBoneCount = boneCount;

			// 256바이트 정렬 상수버퍼 크기
			auto Align256 = [](UINT sz) { return (sz + 255u) & ~255u; };
			const UINT cbSize = Align256(sizeof(XMFLOAT4X4) * m_nBoneCount);

			// 업로드 힙 상수버퍼 생성 (GENERIC_READ 상태)
			// CreateBufferResource(device, cmdList, pData, nBytes, heapType, initialState, **ppUploadBuffer)
			// 선언/정의는 stdafx.h / stdafx.cpp 참고
			m_pd3dBoneCB = CreateBufferResource(pd3dDevice, pd3dCommandList,
				nullptr, cbSize,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr);
		}
	}
#endif
}

void CGameObject::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	XMFLOAT4X4 xmf4x4World;
	XMStoreFloat4x4(&xmf4x4World, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4World)));
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4World, 0);
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 3, &m_xmf3Color, 16);
}
void CGameObject::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, XMFLOAT4X4* pxmf4x4World)
{
	XMFLOAT4X4 xmf4x4World;
	XMStoreFloat4x4(&xmf4x4World, XMMatrixTranspose(XMLoadFloat4x4(pxmf4x4World)));
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4World, 0);
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 3, &m_xmf3Color, 16);
}

void CGameObject::ReleaseShaderVariables()
{
	if (m_pd3dBoneCB) { m_pd3dBoneCB->Release(); m_pd3dBoneCB = nullptr; }
}

void CGameObject::Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera)
{
	OnPrepareRender();
	UpdateShaderVariables(pd3dCommandList);
	
	
	// 스키닝 버퍼가 있으면 그걸, 없으면 씬의 디폴트 본 CB를 b4에 항상 묶는다.
	if (m_pd3dBoneCB) {
		pd3dCommandList->SetGraphicsRootConstantBufferView(4, m_pd3dBoneCB->GetGPUVirtualAddress());
	}
	else {
		// Scene 포인터 얻는 방법이 다르면 프로젝트 스타일에 맞게 가져오세요.
		extern CGameFramework* g_pFramework;
		CScene* scene = g_pFramework ? g_pFramework->GetDevice(), g_pFramework->m_pScene : nullptr;
		if (scene) {
			auto gpuAddr = scene->GetDefaultBoneCBAddress();
			if (gpuAddr) pd3dCommandList->SetGraphicsRootConstantBufferView(4, gpuAddr);
		}
	}
	

	if (m_pShader) m_pShader->Render(pd3dCommandList, pCamera);
	if (m_ppMeshes)
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i]) m_ppMeshes[i]->Render(pd3dCommandList);
		}
	}
}

void CGameObject::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, XMFLOAT4X4* pxmf4x4World)
{
	OnPrepareRender();
	UpdateShaderVariables(pd3dCommandList, pxmf4x4World);

	
	// 스키닝 버퍼가 있으면 그걸, 없으면 씬의 디폴트 본 CB를 b4에 항상 묶는다.
	if (m_pd3dBoneCB) {
		pd3dCommandList->SetGraphicsRootConstantBufferView(4, m_pd3dBoneCB->GetGPUVirtualAddress());
	}
	else {
		// Scene 포인터 얻는 방법이 다르면 프로젝트 스타일에 맞게 가져오세요.
		extern CGameFramework* g_pFramework;
		CScene* scene = g_pFramework ? g_pFramework->GetDevice(), g_pFramework->m_pScene : nullptr;
		if (scene) {
			auto gpuAddr = scene->GetDefaultBoneCBAddress();
			if (gpuAddr) pd3dCommandList->SetGraphicsRootConstantBufferView(4, gpuAddr);
		}
	}
	
	
	if (m_pShader) m_pShader->Render(pd3dCommandList, pCamera);
	if (m_ppMeshes)
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i]) m_ppMeshes[i]->Render(pd3dCommandList);
		}
	}
}

void CGameObject::ReleaseUploadBuffers()
{
	if (m_ppMeshes)
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i]) m_ppMeshes[i]->ReleaseUploadBuffers();
		}
	}
}

void CGameObject::SetPosition(float x, float y, float z)
{
	m_xmf4x4World._41 = x;
	m_xmf4x4World._42 = y;
	m_xmf4x4World._43 = z;
}

void CGameObject::SetPosition(XMFLOAT3 xmf3Position)
{
	SetPosition(xmf3Position.x, xmf3Position.y, xmf3Position.z);
}

XMFLOAT3 CGameObject::GetPosition()
{
	return(XMFLOAT3(m_xmf4x4World._41, m_xmf4x4World._42, m_xmf4x4World._43));
}

XMFLOAT3 CGameObject::GetLook()
{
	return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._31, m_xmf4x4World._32, m_xmf4x4World._33)));
}

XMFLOAT3 CGameObject::GetUp()
{
	return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._21, m_xmf4x4World._22, m_xmf4x4World._23)));
}

XMFLOAT3 CGameObject::GetRight()
{
	return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._11, m_xmf4x4World._12, m_xmf4x4World._13)));
}

void CGameObject::MoveStrafe(float fDistance)
{
	XMFLOAT3 xmf3Position = GetPosition();
	XMFLOAT3 xmf3Right = GetRight();
	xmf3Position = Vector3::Add(xmf3Position, xmf3Right, fDistance);
	CGameObject::SetPosition(xmf3Position);
}

void CGameObject::MoveUp(float fDistance)
{
	XMFLOAT3 xmf3Position = GetPosition();
	XMFLOAT3 xmf3Up = GetUp();
	xmf3Position = Vector3::Add(xmf3Position, xmf3Up, fDistance);
	CGameObject::SetPosition(xmf3Position);
}

void CGameObject::MoveForward(float fDistance)
{
	XMFLOAT3 xmf3Position = GetPosition();
	XMFLOAT3 xmf3Look = GetLook();
	xmf3Position = Vector3::Add(xmf3Position, xmf3Look, fDistance);
	CGameObject::SetPosition(xmf3Position);
}

void CGameObject::Rotate(float fPitch, float fYaw, float fRoll)
{
	XMMATRIX mtxRotate = XMMatrixRotationRollPitchYaw(XMConvertToRadians(fPitch), XMConvertToRadians(fYaw), XMConvertToRadians(fRoll));
	m_xmf4x4World = Matrix4x4::Multiply(mtxRotate, m_xmf4x4World);
}

void CGameObject::Rotate(XMFLOAT3 *pxmf3Axis, float fAngle)
{
	XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(pxmf3Axis), XMConvertToRadians(fAngle));
	m_xmf4x4World = Matrix4x4::Multiply(mtxRotate, m_xmf4x4World);
}

void CGameObject::LookTo(XMFLOAT3& xmf3LookTo, XMFLOAT3& xmf3Up)
{
	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookToLH(GetPosition(), xmf3LookTo, xmf3Up);
	m_xmf4x4World._11 = xmf4x4View._11; m_xmf4x4World._12 = xmf4x4View._21; m_xmf4x4World._13 = xmf4x4View._31;
	m_xmf4x4World._21 = xmf4x4View._12; m_xmf4x4World._22 = xmf4x4View._22; m_xmf4x4World._23 = xmf4x4View._32;
	m_xmf4x4World._31 = xmf4x4View._13; m_xmf4x4World._32 = xmf4x4View._23; m_xmf4x4World._33 = xmf4x4View._33;
}

void CGameObject::LookAt(XMFLOAT3& xmf3LookAt, XMFLOAT3& xmf3Up)
{
	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookAtLH(GetPosition(), xmf3LookAt, xmf3Up);
	m_xmf4x4World._11 = xmf4x4View._11; m_xmf4x4World._12 = xmf4x4View._21; m_xmf4x4World._13 = xmf4x4View._31;
	m_xmf4x4World._21 = xmf4x4View._12; m_xmf4x4World._22 = xmf4x4View._22; m_xmf4x4World._23 = xmf4x4View._32;
	m_xmf4x4World._31 = xmf4x4View._13; m_xmf4x4World._32 = xmf4x4View._23; m_xmf4x4World._33 = xmf4x4View._33;
}

void CGameObject::UpdateBoundingBox()
{
	if (m_ppMeshes)
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i]) m_ppMeshes[i]->m_xmOOBB.Transform(m_xmOOBB, XMLoadFloat4x4(&m_xmf4x4World));
		}
		XMStoreFloat4(&m_xmOOBB.Orientation, XMQuaternionNormalize(XMLoadFloat4(&m_xmOOBB.Orientation)));
	}
}

int CGameObject::PickObjectByRayIntersection(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, float* pfHitDistance)
{
	XMMATRIX xmmtxWorld = XMLoadFloat4x4(&m_xmf4x4World);
	XMMATRIX xmmtxWorldInv = XMMatrixInverse(nullptr, xmmtxWorld);

	// 뷰 행렬 역행렬을 사용해서 Ray Origin (카메라 위치) 추출
	XMMATRIX xmmtxViewInv = XMMatrixInverse(nullptr, xmmtxView);
	XMVECTOR xmvPickRayOrigin = xmmtxViewInv.r[3]; // 카메라 위치

	// Pick Ray의 방향을 World 좌표계 기준으로 변환
	XMVECTOR xmvPickRayDir = XMVector3TransformNormal(xmvPickPosition, xmmtxViewInv);
	xmvPickRayDir = XMVector3Normalize(xmvPickRayDir);

	// 로컬 공간으로 변환
	xmvPickRayOrigin = XMVector3TransformCoord(xmvPickRayOrigin, xmmtxWorldInv);
	xmvPickRayDir = XMVector3TransformNormal(xmvPickRayDir, xmmtxWorldInv);
	xmvPickRayDir = XMVector3Normalize(xmvPickRayDir);

	if (m_ppMeshes) 
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i]) return m_ppMeshes[i]->CheckRayIntersection(xmvPickRayOrigin, xmvPickRayDir, pfHitDistance);
		}
	return 0;
}
void CGameObject::GenerateRayForPicking(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection)
{
	XMMATRIX xmmtxToModel = XMMatrixInverse(NULL, XMLoadFloat4x4(&m_xmf4x4World) * xmmtxView);

	XMFLOAT3 xmf3CameraOrigin(0.0f, 0.0f, 0.0f);
	xmvPickRayOrigin = XMVector3TransformCoord(XMLoadFloat3(&xmf3CameraOrigin), xmmtxToModel);
	xmvPickRayDirection = XMVector3TransformCoord(xmvPickPosition, xmmtxToModel);
	xmvPickRayDirection = XMVector3Normalize(xmvPickRayDirection - xmvPickRayOrigin);
}
void CGameObject::SetRotationTransform(XMFLOAT4X4* pmxf4x4Transform)
{
	m_xmf4x4World._11 = pmxf4x4Transform->_11; m_xmf4x4World._12 = pmxf4x4Transform->_12; m_xmf4x4World._13 = pmxf4x4Transform->_13;
	m_xmf4x4World._21 = pmxf4x4Transform->_21; m_xmf4x4World._22 = pmxf4x4Transform->_22; m_xmf4x4World._23 = pmxf4x4Transform->_23;
	m_xmf4x4World._31 = pmxf4x4Transform->_31; m_xmf4x4World._32 = pmxf4x4Transform->_32; m_xmf4x4World._33 = pmxf4x4Transform->_33;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
