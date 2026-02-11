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

void CGameObject::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	OnPrepareRender(cmd, camera);

	// ★ (1) 매 프레임 per-object CB 갱신 (Player는 override 버전이 호출됨)
	UpdateShaderVariables(cmd);

	// ★ (2) PSO/RootSig/Shader 변수 세팅 (카메라 CB 포함)
	if (auto sh = GetShader())
		sh->Render(cmd, camera, nullptr);

	// PreRender hooks
	for (auto& c : m_components)
		if (c && c->IsEnabled()) c->OnPreRender(cmd);

	// Draw
	if (auto* r = GetRenderer())
		if (r->IsEnabled())
			r->Render(cmd, camera);

	// PostRender hooks
	for (auto& c : m_components)
		if (c && c->IsEnabled()) c->OnPostRender(cmd);
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

void CGameObject::PreRenderComponents(ID3D12GraphicsCommandList* cmd)
{
	if (!m_bComponentsCreated) return;

	for (auto& c : m_components)
	{
		if (c && c->IsEnabled())
			c->OnPreRender(cmd);
	}
}

void CGameObject::PostRenderComponents(ID3D12GraphicsCommandList* cmd)
{
	if (!m_bComponentsCreated) return;

	for (auto& c : m_components)
	{
		if (c && c->IsEnabled())
			c->OnPostRender(cmd);
	}
}