//-----------------------------------------------------------------------------
// File: RenderObjectComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "RenderObjectComponent.h"
#include "Object.h"          // CB_GAMEOBJECT_INFO 정의 필요(=sizeof 때문에)
#include "d3dx12.h"

CRenderObjectComponent::CRenderObjectComponent(CGameObject* owner)
    : CComponentT<CRenderObjectComponent>(owner)
{
}

void CRenderObjectComponent::OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
    (void)cmd;

    // 외부 바인딩이 아직 안 됐으면(플레이어 등) 로컬 CB를 만든다.
    if (!m_mapped)
        CreateLocalCB(dev, cmd);
}

void CRenderObjectComponent::OnDestroy()
{
    // 로컬 소유분만 정리
    if (m_localCB)
    {
        m_localCB->Unmap(0, nullptr);
        m_mapped = nullptr;
        m_localCB.Reset();
    }
    m_cbSizeBytes = 0;
    m_isExternal = false;
}

void CRenderObjectComponent::OnReset()
{
	if ( m_mapped )
	{
		XMStoreFloat4x4(&m_mapped->m_xmf4x4World, XMMatrixTranspose(XMMatrixIdentity()));
		m_mapped->TexTransform = Matrix4x4::Identity();
		m_mapped->ShadowTransform = Matrix4x4::Identity();
		m_mapped->m_nObjectID = m_objectId;
	}
}

void CRenderObjectComponent::BindExternal(CB_GAMEOBJECT_INFO* mapped, D3D12_GPU_DESCRIPTOR_HANDLE cbvGpu)
{
    // 로컬이 있었다면 해제(외부로 전환)
    if (m_localCB)
    {
        m_localCB->Unmap(0, nullptr);
        m_localCB.Reset();
    }

    m_mapped = mapped;
    m_cbvGpu = cbvGpu;
    m_cbSizeBytes = 0;     // 외부는 사이즈를 여기서 굳이 유지 안 해도 됨
    m_isExternal = true;
}

void CRenderObjectComponent::CreateLocalCB(ID3D12Device* dev, ID3D12GraphicsCommandList* /*cmd*/)
{
    if (!dev) 
		return;

    // 이미 로컬이 있으면 스킵
    if (m_localCB && m_mapped) 
		return;

    // 외부로 이미 바인딩 되어 있다면 로컬 만들면 안 됨
    if (m_isExternal && m_mapped) 
		return;

    m_cbSizeBytes = Align256((UINT)sizeof(CB_GAMEOBJECT_INFO));

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC   bufDesc = CD3DX12_RESOURCE_DESC::Buffer(m_cbSizeBytes);

    HRESULT hr = dev->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_localCB.GetAddressOf()));
    if (FAILED(hr))
    {
        m_localCB.Reset();
        m_mapped = nullptr;
        m_cbSizeBytes = 0;
        return;
    }

    m_localCB->Map(0, nullptr, (void**)&m_mapped);

    // 초기값: identity
    if (m_mapped)
    {
        XMStoreFloat4x4(&m_mapped->m_xmf4x4World, XMMatrixTranspose(XMMatrixIdentity()));
        m_mapped->m_nObjectID = m_objectId;
    }

    m_isExternal = false;
}
