//------------------------------------------------------- ----------------------
// File: BaseComponent.h
// 기본 컴포넌트 클래스 - DirectX 의존성 없음
//-----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include "GameMath.h"

// forward declarations
class CBaseObject;
class ID3D12Device;
class ID3D12GraphicsCommandList;

// ============================================================================
// Base Component (Unity-style)
//  - Owner(ServerObject) 포인터 보유
//  - 오버라이드: OnCreate/OnDestroy/OnUpdate 등
//  - RTTI 대용 타입 식별: StaticTypeId<T>
// ============================================================================
class CComponent
{
public:
	using TypeId = const void*;

	template<typename T>
	static TypeId StaticTypeId()
	{
		static int s_tag;
		return &s_tag;
	}

public:
	explicit CComponent(CBaseObject* owner) : m_pOwner(owner) {}
	virtual ~CComponent() = default;

	virtual TypeId GetTypeId() const = 0;

	CBaseObject* GetOwner() const { return m_pOwner; }

	bool IsEnabled() const { return m_bEnabled; }
	void SetEnabled(bool b) { m_bEnabled = b; }

	// Lifecycle
	virtual void OnCreate() {}
	virtual void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) { OnCreate(); }
	virtual void OnDestroy() {}

	// Per-frame tick
	virtual void OnUpdate(float dt) {}
	virtual void OnLateUpdate(float dt) {}

protected:
	CBaseObject* m_pOwner = nullptr;
	bool           m_bEnabled = true;
};

// ----------------------------------------------------------------------------
// CRTP helper: class MyComp : public CComponentT<MyComp>
// ----------------------------------------------------------------------------
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

