//-----------------------------------------------------------------------------
// File: SkinningComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "SkinningComponent.h"

CSkinningComponent::CSkinningComponent(CGameObject* owner)
    : CComponentT<CSkinningComponent>(owner)
{
}

void CSkinningComponent::Enable(ID3D12Device* dev, int nBones)
{
    if (nBones <= 0) { Disable(); return; }

    Disable();

    m_skinned = true;
    m_nBones = nBones;

    const UINT cbSize = Align256((UINT)(sizeof(XMFLOAT4X4) * (UINT)m_nBones));

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    HRESULT hr = dev->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_cbBone.GetAddressOf())
    );
    if (FAILED(hr))
    {
        m_cbBone.Reset();
        m_skinned = false;
        m_nBones = 0;
        return;
    }

    m_cbBone->Map(0, nullptr, (void**)&m_mapped);

    // 초기 identity(전치)
    XMFLOAT4X4 I;
    XMStoreFloat4x4(&I, XMMatrixTranspose(XMMatrixIdentity()));
    for (int i = 0; i < m_nBones; ++i)
        m_mapped[i] = I;
}

void CSkinningComponent::Disable()
{
    m_skinned = false;
    m_nBones = 0;

    if (m_cbBone)
    {
        m_cbBone->Unmap(0, nullptr);
        m_mapped = nullptr;
        m_cbBone.Reset();
    }
}

void CSkinningComponent::Upload(const XMFLOAT4X4* boneMats, int nMats)
{
    if (!m_skinned || !m_mapped || !boneMats) return;

    int count = (nMats < m_nBones) ? nMats : m_nBones;

    for (int i = 0; i < count; ++i)
    {
        XMMATRIX m = XMLoadFloat4x4(&boneMats[i]);
        m = XMMatrixTranspose(m);
        XMStoreFloat4x4(&m_mapped[i], m);
    }
}
