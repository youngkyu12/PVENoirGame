//-----------------------------------------------------------------------------
// File: CGameObject_Render.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Object.h"
#include "AnimController.h"
#include "AnimatorComponent.h"

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
}

void CGameObject::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
}

void CGameObject::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	OnPrepareRender(cmd, camera);

	UpdateShaderVariables(cmd);

	for (std::unique_ptr<CComponent>& c : m_components)
		if (c && c->IsEnabled()) c->OnPreRender(cmd);

	CRendererComponent* renderer = GetRenderer();
	if (renderer && renderer->IsEnabled())
		renderer->Render(cmd, camera);

	for (std::unique_ptr<CComponent>& c : m_components)
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

	for (std::unique_ptr<CComponent>& c : m_components)
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