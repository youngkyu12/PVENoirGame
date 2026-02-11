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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CGameObject::CGameObject(int nMeshes)
{
    m_components.reserve(8);

    m_pTransform = AddComponent<CTransformComponent>();

    m_pModel = AddComponent<CModelComponent>(nMeshes);

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


void CGameObject::EnableSkinning(ID3D12Device* pd3dDevice, int nBones)
{
    if (nBones <= 0)
    {
        DisableSkinning();
        return;
    }

    m_bSkinnedObject = true;
    m_nBones = nBones;

    // 이미 생성되어 있으면 정리
    if (m_pd3dcbBoneTransforms)
    {
        m_pd3dcbBoneTransforms->Unmap(0, NULL);
        m_pcbMappedBoneTransforms = NULL;
        m_pd3dcbBoneTransforms.Reset();
    }

    // Bone palette CB 생성 (Upload heap, 256-byte align)
    UINT cbSize = (sizeof(CB_BONE_PALETTE) + 255) & ~255;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    HRESULT hr = pd3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_pd3dcbBoneTransforms.GetAddressOf())
    );

    if (FAILED(hr))
    {
        OutputDebugStringA("[CGameObject::EnableSkinning] Failed to create bone CB.\n");
        DisableSkinning();
        return;
    }

    // persistent map
    m_pd3dcbBoneTransforms->Map(0, nullptr, (void**)&m_pcbMappedBoneTransforms);

    // 초기값: identity(전치해서 저장: HLSL mul(pos, M) 패턴과 기존 코드 일관성)
    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixTranspose(XMMatrixIdentity()));

    for (int i = 0; i < m_nBones; ++i)
        m_pcbMappedBoneTransforms[i] = identity;
}

void CGameObject::DisableSkinning()
{
    m_bSkinnedObject = false;
    m_nBones = 0;

    if (m_pd3dcbBoneTransforms)
    {
        m_pd3dcbBoneTransforms->Unmap(0, NULL);
        m_pcbMappedBoneTransforms = NULL;
        m_pd3dcbBoneTransforms.Reset();
    }
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
    return (m_pd3dcbBoneTransforms) ? m_pd3dcbBoneTransforms->GetGPUVirtualAddress() : 0;
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



void CGameObject::UpdateBoneTransformsOnGPU(const XMFLOAT4X4* pxmf4x4BoneTransforms, int nBones)
{
    if (!m_bSkinnedObject || !m_pcbMappedBoneTransforms || !pxmf4x4BoneTransforms)
        return;

    int count = (nBones < m_nBones) ? nBones : m_nBones;

    // 기존 Mesh.cpp 로직과 동일: 전치해서 업로드
    for (int i = 0; i < count; ++i)
    {
        XMMATRIX m = XMLoadFloat4x4(&pxmf4x4BoneTransforms[i]);
        m = XMMatrixTranspose(m);
        XMStoreFloat4x4(&m_pcbMappedBoneTransforms[i], m);
    }
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