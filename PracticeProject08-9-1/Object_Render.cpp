//-----------------------------------------------------------------------------
// File: CGameObject_Render.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Object.h"
#include "AnimController.h"

void CGameObject::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	(void)pd3dCommandList;
	if (!m_pcbMappedGameObject) return;

	// Transform이 WorldMatrix의 유일 권위
	const XMFLOAT4X4& W = m_pTransform->GetWorldMatrix();

	XMStoreFloat4x4(
		&m_pcbMappedGameObject->m_xmf4x4World,
		XMMatrixTranspose(XMLoadFloat4x4(&W))
	);

	m_pcbMappedGameObject->m_nObjectID = 0;

	// (옵션) 캐시를 쓰는 경우에만 동기화
	// m_cachedWorld = W;
}




void CGameObject::Animate(float dt)
{
	UpdateComponents(dt);

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
	XMFLOAT3 delta = Vector3::ScalarProduct(GetRight(), fDistance, false);
	m_pTransform->Translate(delta);
}

void CGameObject::MoveUp(float fDistance)
{
	XMFLOAT3 delta = Vector3::ScalarProduct(GetUp(), fDistance, false); // 로컬업
	m_pTransform->Translate(delta);
}

void CGameObject::MoveForward(float fDistance)
{
	XMFLOAT3 delta = Vector3::ScalarProduct(GetLook(), fDistance, false);
	m_pTransform->Translate(delta);
}

void CGameObject::Rotate(float fPitch, float fYaw, float fRoll)
{
	// Transform이 회전의 유일 권위
	m_pTransform->RotateWorldEulerDegrees(fPitch, fYaw, fRoll);
}

void CGameObject::Rotate(XMFLOAT3* pxmf3Axis, float fAngle)
{
	if (!pxmf3Axis) return;
	m_pTransform->RotateWorldAxisDegrees(*pxmf3Axis, fAngle);
}
