//-----------------------------------------------------------------------------
// File: RenderObjectComponent.h
//-----------------------------------------------------------------------------
#pragma once
#include "BaseComponent.h"
#include <wrl.h>
using Microsoft::WRL::ComPtr;

struct CB_GAMEOBJECT_INFO;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

class CRenderObjectComponent final : public CComponentT<CRenderObjectComponent>
{
public:
    explicit CRenderObjectComponent(CGameObject* owner);

    void BindExternal(CB_GAMEOBJECT_INFO* mapped, D3D12_GPU_DESCRIPTOR_HANDLE cbvGpu);

    // 로컬 CB(플레이어) 생성/맵핑
    void CreateLocalCB(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd);

    CB_GAMEOBJECT_INFO* GetMappedCB() const { return m_mapped; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetCbvHandle() const { return m_cbvGpu; }

    void SetCbvHandle(D3D12_GPU_DESCRIPTOR_HANDLE h) { m_cbvGpu = h; }
    void SetCbvHandlePtr(UINT64 ptr) { m_cbvGpu.ptr = ptr; }

    ID3D12Resource* GetCBResource() const { return m_localCB.Get(); }
    UINT GetCBSizeBytes() const { return m_cbSizeBytes; }

    void SetObjectID(UINT id) { m_objectId = id; }
    UINT GetObjectID() const { return m_objectId; }

    void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;
    void OnDestroy() override;

private:
    static UINT Align256(UINT x) { return (x + 255u) & ~255u; }

private:
    D3D12_GPU_DESCRIPTOR_HANDLE m_cbvGpu = { 0 };
    CB_GAMEOBJECT_INFO* m_mapped = nullptr;

    // 로컬 CB(플레이어)일 때만 소유
    ComPtr<ID3D12Resource>      m_localCB;
    UINT                       m_cbSizeBytes = 0;

    UINT                       m_objectId = 0;
    bool                       m_isExternal = false;
};
