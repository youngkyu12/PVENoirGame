//-----------------------------------------------------------------------------
// File: RendererComponent.h
//-----------------------------------------------------------------------------

#pragma once

#include "Component.h"

// forward
class CCamera;

// ============================================================================
// Renderer Component Base
//  - CComponent를 "한 번만" 상속 (다중상속/모호성 제거)
//  - Render()만 순수 가상으로 제공
// ============================================================================
class CRendererComponent : public CComponent
{
public:
    explicit CRendererComponent(CGameObject* owner) : CComponent(owner) {}
    virtual ~CRendererComponent() = default;

    bool IsRenderer() const override { return true; }

    virtual void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera) = 0;
};

// ----------------------------------------------------------------------------
// CRTP helper: renderer용 TypeId 제공
// ----------------------------------------------------------------------------
template<typename TDerived>
class CRendererComponentT : public CRendererComponent
{
public:
    using CRendererComponent::CRendererComponent;

    TypeId GetTypeId() const override
    {
        return CComponent::StaticTypeId<TDerived>();
    }
};

// ----------------------------------------------------------------------------
// Static / Skinned renderer
// ----------------------------------------------------------------------------
class CStaticMeshRendererComponent final : public CRendererComponentT<CStaticMeshRendererComponent>
{
public:
    explicit CStaticMeshRendererComponent(CGameObject* owner)
        : CRendererComponentT(owner) {
    }

    void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera) override;
};

class CSkinnedMeshRendererComponent final : public CRendererComponentT<CSkinnedMeshRendererComponent>
{
public:
    explicit CSkinnedMeshRendererComponent(CGameObject* owner)
        : CRendererComponentT(owner) {
    }

    void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera) override;
};

class CColliderMeshRendererComponent final : public CRendererComponentT<CColliderMeshRendererComponent>
{
public:
	explicit CColliderMeshRendererComponent(CGameObject* owner)
		: CRendererComponentT(owner) {
	}

	void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera) override;
};