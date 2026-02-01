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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CGameObject::CGameObject(int nMeshes)
{
	m_nMeshes = nMeshes;
	if (m_nMeshes > 0)
	{
		m_ppMeshes.resize(m_nMeshes);
	}
}

CGameObject::~CGameObject()
{
	ReleaseShaderVariables();

	if (!m_ppMeshes.empty())
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i])
				m_ppMeshes[i].reset();
		}
	}
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

	if (m_pMaterial)
		m_pMaterial->ReleaseShaderVariables();
}

void CGameObject::ReleaseUploadBuffers()
{
	if (!m_ppMeshes.empty())
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i])m_ppMeshes[i]->ReleaseUploadBuffers();
		}
	}

	if (m_pMaterial)m_pMaterial->ReleaseUploadBuffers();
}

void CGameObject::SetMesh(int nIndex, shared_ptr<CMesh> pMesh)
{
	if (!m_ppMeshes.empty())
	{
		if (m_ppMeshes[nIndex])
			m_ppMeshes[nIndex].reset();
		m_ppMeshes[nIndex] = pMesh;
	}
}

void CGameObject::SetShader(shared_ptr<CShader> pShader)
{
	if (!m_pMaterial)
	{
		shared_ptr<CMaterial> pMaterial = make_shared<CMaterial>();
		SetMaterial(pMaterial);
	}
	if (m_pMaterial)
		m_pMaterial->SetShader(pShader);
}

void CGameObject::SetMaterial(shared_ptr<CMaterial> pMaterial)
{
	if (m_pMaterial)
		m_pMaterial.reset();
	m_pMaterial = pMaterial;
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
    UINT cbSize = (UINT)(sizeof(XMFLOAT4X4) * m_nBones);
    cbSize = (cbSize + 255) & ~255;

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
    m_pd3dcbBoneTransforms->Map(0, NULL, (void**)&m_pcbMappedBoneTransforms);

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

    if (m_ppMeshes.empty() || !m_ppMeshes[0])
        return empty;

    return m_ppMeshes[0]->GetBones();
}

const std::unordered_map<std::string, int>& CGameObject::GetBoneNameToIndex() const
{
    static const std::unordered_map<std::string, int> empty;

    if (m_ppMeshes.empty() || !m_ppMeshes[0])
        return empty;

    return m_ppMeshes[0]->GetBoneNameToIndex();
}

D3D12_GPU_VIRTUAL_ADDRESS CGameObject::GetBoneCBAddress() const
{
    return (m_pd3dcbBoneTransforms) ? m_pd3dcbBoneTransforms->GetGPUVirtualAddress() : 0;
}

CAnimator* CGameObject::EnsureAnimator()
{
    if (!m_pAnimator)
    {
        m_pAnimator = std::make_unique<CAnimator>();

        // 메시(스켈레톤 메타) 기반으로 Animator 초기화
        const auto& bones = GetBones();
        const auto& map = GetBoneNameToIndex();
        if (!bones.empty() && !map.empty())
            m_pAnimator->SetSkeleton(bones, map);
    }
    return m_pAnimator.get();
}

void CGameObject::PlayAnimation(const std::string& clipName, bool loop, float start)
{
    // 1) Animator는 오브젝트 소유
    CAnimator* anim = EnsureAnimator();
    if (!anim) return;

    // 2) 재생 (여기서 start 시점 포즈 계산/세팅을 한다는 전제는 네 Animator 구현에 따름)
    if (!anim->Play(clipName, loop, start))
        return;

    // 3) 스키닝 오브젝트가 아니거나, bone CB가 아직 없으면 여기서는 할 수 있는 게 없음
    //    (EnableSkinning(pd3dDevice, nBones)는 외부에서 이미 호출돼 있어야 함)
    if (!m_bSkinnedObject || !m_pd3dcbBoneTransforms)
        return;

    // 4) Animator가 만든 최종 본 행렬을 "오브젝트의" b7 CB에 업로드
    const auto& mats = anim->GetFinalBoneMatrices();
    if (mats.empty())
        return;

    UpdateBoneTransformsOnGPU(mats.data(), static_cast<int>(mats.size()));
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
    if (!m_pAnimController)
    {
        m_pAnimController = std::make_unique<CAnimController>();
        m_pAnimController->Bind(this);
    }
    return m_pAnimController.get();
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//
CRotatingObject::CRotatingObject(int nMeshes)
{
}

CRotatingObject::~CRotatingObject()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
CRevolvingObject::CRevolvingObject(int nMeshes)
{
}

CRevolvingObject::~CRevolvingObject()
{
}


