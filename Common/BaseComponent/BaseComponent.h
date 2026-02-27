//-----------------------------------------------------------------------------
// File: BaseComponent.h  (COMMON)
//-----------------------------------------------------------------------------

#pragma once
#include <cstdint>


// Owner forward declare
class COMMON_OWNER_TYPE;

// D3D forward declare (헤더 include 없이 선언만; 서버에서도 문제 없음)
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

class CComponent
{
public:
    using TypeId = const void*;
    using OwnerT = COMMON_OWNER_TYPE;

    template<typename T>
    static TypeId StaticTypeId()
    {
        static int s_tag;
        return &s_tag;
    }

public:
    explicit CComponent(OwnerT* owner) : m_pOwner(owner) {}
    virtual ~CComponent() = default;

    virtual TypeId GetTypeId() const = 0;

    OwnerT* GetOwner() const { return m_pOwner; }

    // 클라에서 Renderer 캐시용으로 쓰던 훅(서버는 무시)
    virtual bool IsRenderer() const { return false; }

    bool IsEnabled() const { return m_bEnabled; }
    void SetEnabled(bool b) { m_bEnabled = b; }

    // 서버 공용 init
    virtual void OnCreate() {}

    // 클라 init (기본은 OnCreate()로 위임)
    virtual void OnCreate(ID3D12Device*, ID3D12GraphicsCommandList*) { OnCreate(); }

    virtual void OnDestroy() {}

    virtual void OnUpdate(float) {}
    virtual void OnLateUpdate(float) {}

    virtual void OnPreRender(ID3D12GraphicsCommandList*) {}
    virtual void OnPostRender(ID3D12GraphicsCommandList*) {}

protected:
    OwnerT* m_pOwner = nullptr;
    bool    m_bEnabled = true;
};

template<typename TDerived>
class CComponentT : public CComponent
{
public:
    using CComponent::CComponent;

    TypeId GetTypeId() const override
    {
        return CComponent::StaticTypeId<TDerived>();
    }
};