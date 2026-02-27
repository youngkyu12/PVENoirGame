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

void CGameObject::SetMesh(int nIndex, shared_ptr<CMesh> pMesh)
{
    if (!m_pModel) return;
    m_pModel->SetMesh(nIndex, std::move(pMesh));
    if (auto* ac = GetAnimatorComponent())
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
    if (m_pSkinning && m_pSkinning->IsSkinned())
        cmd->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_BONE_PALETTE, m_pSkinning->GetBoneCBAddress());
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

    for (auto& c : m_components)
    {
        if (c && c->IsEnabled())
            c->OnCreate(pd3dDevice, pd3dCommandList);
    }
}

void CGameObject::DestroyComponents()
{
    if (!m_bComponentsCreated && m_components.empty())
        return;

    // 역순 파괴(의존 관계 대비)
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

    for (auto& c : m_components)
    {
        if (c && c->IsEnabled())
            c->OnUpdate(dt);
    }
    for (auto& c : m_components)
    {
        if (c && c->IsEnabled())
            c->OnLateUpdate(dt);
    }
}


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

CAnimatorComponent* CGameObject::EnsureAnimatorComponent()
{
    if (auto* ac = GetAnimatorComponent())
        return ac;

    // AnimatorComponent는 기본 컴포넌트가 아니므로 "필요할 때만" 붙인다.
    auto* ac = AddComponent<CAnimatorComponent>();
    m_pAnimatorComponent = ac;
    return ac;
}

CAnimator* CGameObject::EnsureAnimator()
{
    auto* ac = EnsureAnimatorComponent();
    return ac ? ac->EnsureAnimator() : nullptr;
}

CAnimator* CGameObject::GetAnimator() const
{
    auto* ac = GetAnimatorComponent();
    return ac ? ac->GetAnimator() : nullptr;
}

void CGameObject::PlayAnimation(const std::string& clipName, bool loop, float start)
{
    auto* ac = EnsureAnimatorComponent();
    if (!ac) return;

    // 재생 명령만 내린다 (업로드/평가 흐름은 컴포넌트가 담당)
    ac->Play(clipName, loop, start);
}



void CGameObject::UpdateBoneTransformsOnGPU(const XMFLOAT4X4* mats, int nBones)
{
    if (m_pSkinning) m_pSkinning->Upload(mats, nBones);
}

void CGameObject::SetColliderType(const EColliderType Type)
{
    mColliderType = Type;
}

void CGameObject::SetMappedGameObjectCB(CB_GAMEOBJECT_INFO* p)
{
    if (!m_pRenderObject) m_pRenderObject = AddComponent<CRenderObjectComponent>();
    // 외부 CB만 바인딩할 수도 있으니 handle은 유지
    m_pRenderObject->BindExternal(p, m_pRenderObject->GetCbvHandle());
}

CB_GAMEOBJECT_INFO* CGameObject::GetMappedGameObjectCB() const
{
    return m_pRenderObject ? m_pRenderObject->GetMappedCB() : nullptr;
}

CAnimController* CGameObject::EnsureAnimController()
{
    auto* ac = EnsureAnimatorComponent();
    return ac ? ac->EnsureController() : nullptr;
}

CAnimController* CGameObject::GetAnimController() const
{
    auto* ac = GetAnimatorComponent();
    return ac ? ac->GetController() : nullptr;
}

CAnimatorComponent* CGameObject::GetAnimatorComponent()
{
    if (m_pAnimatorComponent) return m_pAnimatorComponent;

    const auto want = CComponent::StaticTypeId<CAnimatorComponent>();
    for (auto& c : m_components)
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