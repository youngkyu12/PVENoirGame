//-----------------------------------------------------------------------------
// File:Object.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Object.h"
#include "Shader.h"
#include "Texture.h"
#include "Material.h"

#include "Scene.h"
#include "Animator.h"
#include "AnimController.h"
#include "AnimatorComponent.h"
#include "RenderObjectComponent.h"
#include "SkinningComponent.h"
#include "ColliderComponent.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CGameObject::CGameObject(int nMeshes)
{
	m_components.reserve(8);
	m_pTransform = AddComponent<CTransformComponent>();
	m_pModel = AddComponent<CModelComponent>(nMeshes);
	m_pRenderObject = AddComponent<CRenderObjectComponent>();
	m_pSkinning = AddComponent<CSkinningComponent>();

	m_pRenderer = nullptr;
}



CGameObject::~CGameObject()
{
	DestroyComponents();

	ReleaseShaderVariables();
}


void CGameObject::ReleaseShaderVariables()
{
	if (m_pd3dcbGameObject)
	{
		m_pd3dcbGameObject->Unmap(0, NULL);
		m_pcbMappedGameObject = NULL;
		m_pd3dcbGameObject.Reset();
	}

	if (m_pd3dcbBoneTransforms)
	{
		m_pd3dcbBoneTransforms->Unmap(0, NULL);
		m_pcbMappedBoneTransforms = NULL;
		m_pd3dcbBoneTransforms.Reset();
	}

	m_bSkinnedObject = false;
	m_nBones = 0;
}

void CGameObject::ReleaseUploadBuffers()
{
	if (m_pModel)
		m_pModel->ReleaseUploadBuffers();
}

// ============================================================================
// Mesh / RenderObject bindings
// ============================================================================
void CGameObject::SetMesh(int nIndex, shared_ptr<CMesh> pMesh)
{
	if (!m_pModel) return;
	m_pModel->SetMesh(nIndex, std::move(pMesh));
	if (CAnimatorComponent* ac = GetAnimatorComponent())
		ac->InvalidateSkeleton();
}

void CGameObject::SetCbvGPUDescriptorHandle(D3D12_GPU_DESCRIPTOR_HANDLE h)
{
	if (!m_pRenderObject) m_pRenderObject = AddComponent<CRenderObjectComponent>();
	m_pRenderObject->SetCbvHandle(h);
}

void CGameObject::SetCbvGPUDescriptorHandlePtr(UINT64 ptr)
{
	if (!m_pRenderObject) m_pRenderObject = AddComponent<CRenderObjectComponent>();
	m_pRenderObject->SetCbvHandlePtr(ptr);
}

D3D12_GPU_DESCRIPTOR_HANDLE CGameObject::GetCbvGPUDescriptorHandle()
{
	return m_pRenderObject ? m_pRenderObject->GetCbvHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
}

void CGameObject::SetRootParameter(ID3D12GraphicsCommandList* cmd)
{
	// b2 table: per-object CBV
	if (m_pRenderObject)
		cmd->SetGraphicsRootDescriptorTable(ROOT_PARAMETER_OBJECT, m_pRenderObject->GetCbvHandle());

	// b7: bone palette
	if ( m_pSkinning && m_pSkinning->IsSkinned() )
		cmd->SetGraphicsRootShaderResourceView(ROOT_PARAMETER_BONE_PALETTE, m_pSkinning->GetBoneCBAddress());
}

void CGameObject::SetMappedGameObjectCB(CB_GAMEOBJECT_INFO* p)
{
	if (!m_pRenderObject) m_pRenderObject = AddComponent<CRenderObjectComponent>();
	m_pRenderObject->BindExternal(p, m_pRenderObject->GetCbvHandle());
}

CB_GAMEOBJECT_INFO* CGameObject::GetMappedGameObjectCB() const
{
	return m_pRenderObject ? m_pRenderObject->GetMappedCB() : nullptr;
}

// ============================================================================
// Components
// ============================================================================
void CGameObject::CreateComponents(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (m_bComponentsCreated) return;

	m_bComponentsCreated = true;
	m_pd3dDeviceForComponents = pd3dDevice;
	m_pd3dCmdForComponents = pd3dCommandList;

	for (std::unique_ptr<CComponent>& c : m_components)
	{
		if (c && c->IsEnabled())
			c->OnCreate(pd3dDevice, pd3dCommandList);
	}
}

void CGameObject::DestroyComponents()
{
	if (!m_bComponentsCreated && m_components.empty())
		return;

	for (int i = (int)m_components.size() - 1; i >= 0; --i)
	{
		if (m_components[i])
			m_components[i]->OnDestroy();
	}

	m_components.clear();
	m_pRenderer = nullptr;
	m_pTransform = nullptr;
	m_pAnimatorComponent = nullptr;
	m_pModel = nullptr;
	m_pRenderObject = nullptr;
	m_pSkinning = nullptr;
	m_bComponentsCreated = false;
	m_pd3dDeviceForComponents = nullptr;
	m_pd3dCmdForComponents = nullptr;
}

void CGameObject::UpdateComponents(float dt)
{
	if (!m_bComponentsCreated) return;

	for (std::unique_ptr<CComponent>& c : m_components)
	{
		if (c && c->IsEnabled())
			c->OnUpdate(dt);
	}
	for (std::unique_ptr<CComponent>& c : m_components)
	{
		if (c && c->IsEnabled())
			c->OnLateUpdate(dt);
	}
}

CRendererComponent* CGameObject::GetRenderer()
{
	if (m_pRenderer) return m_pRenderer;

	for (auto& c : m_components)
	{
		if (c && c->IsRenderer())
		{
			m_pRenderer = static_cast<CRendererComponent*>(c.get());
			return m_pRenderer;
		}
	}
	return nullptr;
}

const CRendererComponent* CGameObject::GetRenderer() const
{
	return const_cast<CGameObject*>(this)->GetRenderer();
}

CAnimatorComponent* CGameObject::GetAnimatorComponent()
{
	if (m_pAnimatorComponent) return m_pAnimatorComponent;

	const CComponent::TypeId want = CComponent::StaticTypeId<CAnimatorComponent>();
	for (std::unique_ptr<CComponent>& c : m_components)
	{
		if (c && c->GetTypeId() == want)
		{
			m_pAnimatorComponent = static_cast<CAnimatorComponent*>(c.get());
			return m_pAnimatorComponent;
		}
	}
	return nullptr;
}

const CAnimatorComponent* CGameObject::GetAnimatorComponent() const
{
	return const_cast<CGameObject*>(this)->GetAnimatorComponent();
}

CAnimatorComponent* CGameObject::EnsureAnimatorComponent()
{
	if (CAnimatorComponent* ac = GetAnimatorComponent())
		return ac;

	CAnimatorComponent* ac = AddComponent<CAnimatorComponent>();
	m_pAnimatorComponent = ac;
	return ac;
}

// ============================================================================
// Animation / Skinning
// ============================================================================
void CGameObject::EnableSkinning(ID3D12Device* dev, int nBones)
{
	if (!m_pSkinning) m_pSkinning = AddComponent<CSkinningComponent>();
	m_pSkinning->Enable(dev, nBones);
}

void CGameObject::DisableSkinning()
{
	if (m_pSkinning) m_pSkinning->Disable();
}

bool CGameObject::IsSkinnedObject() const
{
	return m_pSkinning && m_pSkinning->IsSkinned();
}

int CGameObject::GetBoneCount() const
{
	return m_pSkinning ? m_pSkinning->GetBoneCount() : 0;
}

const std::vector<Bone>& CGameObject::GetBones() const
{
	static const std::vector<Bone> empty;
	return (m_pModel) ? m_pModel->GetBones() : empty;
}

const std::unordered_map<std::string, int>& CGameObject::GetBoneNameToIndex() const
{
	static const std::unordered_map<std::string, int> empty;
	return (m_pModel) ? m_pModel->GetBoneNameToIndex() : empty;
}


D3D12_GPU_VIRTUAL_ADDRESS CGameObject::GetBoneCBAddress() const
{
	return m_pSkinning ? m_pSkinning->GetBoneCBAddress() : 0;
}

CAnimator* CGameObject::EnsureAnimator()
{
	CAnimatorComponent* ac = EnsureAnimatorComponent();
	return ac ? ac->EnsureAnimator() : nullptr;
}

CAnimator* CGameObject::GetAnimator() const
{
	const CAnimatorComponent* ac = GetAnimatorComponent();
	return ac ? ac->GetAnimator() : nullptr;
}

void CGameObject::PlayAnimation(const std::string& clipName, bool loop, float start)
{
	CAnimatorComponent* ac = EnsureAnimatorComponent();
	if (!ac) return;

	ac->Play(clipName, loop, start);
}

void CGameObject::UpdateBoneTransformsOnGPU(const XMFLOAT4X4* mats, int nBones)
{
	if (m_pSkinning) m_pSkinning->Upload(mats, nBones);
}

CAnimController* CGameObject::EnsureAnimController()
{
	CAnimatorComponent* ac = EnsureAnimatorComponent();
	return ac ? ac->EnsureController() : nullptr;
}

CAnimController* CGameObject::GetAnimController() const
{
	const CAnimatorComponent* ac = GetAnimatorComponent();
	return ac ? ac->GetController() : nullptr;
}

// ============================================================================
// Shader Variables
// ============================================================================
void CGameObject::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	UINT ncbElementBytes = ((sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255);
	m_pd3dcbGameObject = ::CreateBufferResource(
		pd3dDevice,
		pd3dCommandList,
		nullptr,
		ncbElementBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	m_pd3dcbGameObject->Map(0, nullptr, (void**)&m_pcbMappedGameObject);

	int nBones = (m_pModel) ? m_pModel->GetMaxBoneCount() : 0;

	if (nBones > 0)
		EnableSkinning(pd3dDevice, nBones);
}

void CGameObject::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	(void)pd3dCommandList;
	if (!m_pcbMappedGameObject) return;

	const XMFLOAT4X4& W = m_pTransform->GetWorldMatrix();

	XMStoreFloat4x4(
		&m_pcbMappedGameObject->m_xmf4x4World,
		XMMatrixTranspose(XMLoadFloat4x4(&W))
	);

	m_pcbMappedGameObject->m_nObjectID = 0;
}

// ============================================================================
// Frame / Render
// ============================================================================
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

// ============================================================================
// Movement helpers
// ============================================================================
void CGameObject::MoveStrafe(float fDistance)
{
	XMFLOAT3 delta = Vector3::ScalarProduct(GetRight(), fDistance, false);
	m_pTransform->Translate(delta);
	if ( m_pCollider )
		m_pCollider->UpdateWorldBounds();
}

void CGameObject::MoveUp(float fDistance)
{
	XMFLOAT3 delta = Vector3::ScalarProduct(GetUp(), fDistance, false);
	m_pTransform->Translate(delta);
	if ( m_pCollider )
		m_pCollider->UpdateWorldBounds();
}

void CGameObject::MoveForward(float fDistance)
{
	XMFLOAT3 delta = Vector3::ScalarProduct(GetLook(), fDistance, false);
	m_pTransform->Translate(delta);
	if ( m_pCollider )
		m_pCollider->UpdateWorldBounds();
}

void CGameObject::Rotate(float fPitch, float fYaw, float fRoll)
{
	m_pTransform->RotateWorldEulerDegrees(fPitch, fYaw, fRoll);
	if ( m_pCollider )
		m_pCollider->UpdateWorldBounds();
}

void CGameObject::Rotate(XMFLOAT3* pxmf3Axis, float fAngle)
{
	if (!pxmf3Axis) return;
	m_pTransform->RotateWorldAxisDegrees(*pxmf3Axis, fAngle);
	if ( m_pCollider )
		m_pCollider->UpdateWorldBounds();
}

// ============================================================================
// Unused helpers (kept as-is; simply grouped)
// ============================================================================
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