//-----------------------------------------------------------------------------
// File: SkinningComponent.h
//-----------------------------------------------------------------------------
#pragma once

#include "Component.h"
#include <wrl.h>

using Microsoft::WRL::ComPtr;

struct ID3D12Device;

class CSkinningComponent final : public CComponentT<CSkinningComponent>
{
public:
    explicit CSkinningComponent(CGameObject* owner);

    void Enable(ID3D12Device* dev, int nBones);
    void Disable();

    bool IsSkinned() const { return m_skinned; }
    int  GetBoneCount() const { return m_nBones; }

    D3D12_GPU_VIRTUAL_ADDRESS GetBoneCBAddress() const
    {
        return m_cbBone ? m_cbBone->GetGPUVirtualAddress() : 0;
    }

    void Upload(const XMFLOAT4X4* boneMats, int nMats);

    void OnDestroy() override { Disable(); }

	const XMFLOAT4X4* GetMappedBoneMatrices() const { return m_mapped; }

private:
    static UINT Align256(UINT x) { return (x + 255u) & ~255u; }

private:
    bool m_skinned = false;
    int  m_nBones = 0;

    ComPtr<ID3D12Resource> m_cbBone;
    XMFLOAT4X4* m_mapped = nullptr;
};
