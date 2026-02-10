//-----------------------------------------------------------------------------
// File: CGameObject_Render.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Object.h"
#include "AnimController.h"

void CGameObject::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (!m_pcbMappedGameObject) return;

	XMStoreFloat4x4(
		&m_pcbMappedGameObject->m_xmf4x4World,
		XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4World))
	);

	m_pcbMappedGameObject->m_nObjectID = 0;
}

void CGameObject::Animate(float dt)
{
	// 1) 상태 결정(Idle/Run)
	if (m_pAnimController)
		m_pAnimController->Update(dt);

	// 2) 포즈 계산
	if (m_pAnimator)
	{
		m_pAnimator->Update(dt);

		// 3) 스키닝이면 GPU 업로드
		if (m_bSkinnedObject && m_pd3dcbBoneTransforms)
		{
			const auto& mats = m_pAnimator->GetFinalBoneMatrices();
			if (!mats.empty())
				UpdateBoneTransformsOnGPU(mats.data(), (int)mats.size());
		}
	}
}

void CGameObject::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
}

void CGameObject::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	OnPrepareRender(pd3dCommandList, pCamera);
	(void)pCamera; // 더 이상 여기서 카메라를 건드리지 않음

	// Draw-only: per-object root binding만
	SetRootParameter(pd3dCommandList);

	// Draw-only: mesh draw만
	if (!m_ppMeshes.empty())
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i])
				m_ppMeshes[i]->Render(pd3dCommandList, m_pcbMappedGameObject);
		}
	}
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

void CGameObject::Rotate(XMFLOAT3* pxmf3Axis, float fAngle)
{
	XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(pxmf3Axis), XMConvertToRadians(fAngle));
	m_xmf4x4World = Matrix4x4::Multiply(mtxRotate, m_xmf4x4World);
}
