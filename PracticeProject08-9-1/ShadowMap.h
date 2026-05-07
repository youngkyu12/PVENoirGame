//-----------------------------------------------------------------------------
// File: ShadowMap.h
//-----------------------------------------------------------------------------

#pragma once

#include "stdafx.h"

class CGameObject;
class CDescriptorHeap;

struct CB_SHADOW
{
	XMFLOAT4X4 shadowViewProj{};
	XMFLOAT4X4 shadowTransform{};

	XMFLOAT4 shadowLightPos = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	// x = shadow map size
	// y = constant depth bias
	// z = normal/slope bias
	// w = shadow intensity
	XMFLOAT4 shadowParams0 = XMFLOAT4(2048.0f, 0.0008f, 0.0040f, 1.0f);

	// x = shadow map SRV index
	// y = shadow enabled
	// z/w = reserved
	XMUINT4 shadowParams1 = XMUINT4(UINT_MAX, 0u, 0u, 0u);
};

class CShadowMapSystem
{
public:
	CShadowMapSystem() = default;
	~CShadowMapSystem();

public:
	void BuildResources(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		CDescriptorHeap* descriptorHeap
	);

	void ReleaseResources();
	void BindConstantBuffer(ID3D12GraphicsCommandList* cmd) const;

	void UpdateData(
		CGameObject* focusObject,
		CGameObject* directionalLightObject
	);

	void UploadConstantBuffer();

	bool BeginRender(
		ID3D12GraphicsCommandList* cmd,
		ID3D12RootSignature* rootSignature,
		CDescriptorHeap* descriptorHeap,
		D3D12_GPU_VIRTUAL_ADDRESS materialCbGpuAddress
	);

	void EndRender(ID3D12GraphicsCommandList* cmd);

	bool IsReady() const
	{
		return
			m_pd3dShadowMap &&
			m_pd3dShadowDsvHeap &&
			m_pd3dcbShadow &&
			m_pcbMappedShadow &&
			m_shadowMapSrvIndex != UINT_MAX;
	}

	bool IsWorldOOBBInsideShadowBox(const BoundingOrientedBox& box) const;

public:
	UINT GetSrvIndex() const { return m_shadowMapSrvIndex; }
	ID3D12Resource* GetResource() const { return m_pd3dShadowMap.Get(); }
	D3D12_VIEWPORT GetViewport() const { return m_shadowViewport; }
	D3D12_RECT GetScissorRect() const { return m_shadowScissorRect; }
	const CB_SHADOW& GetData() const { return m_shadowData; }

private:
	ComPtr<ID3D12DescriptorHeap> m_pd3dShadowDsvHeap;
	ComPtr<ID3D12Resource> m_pd3dShadowMap;

	ComPtr<ID3D12Resource> m_pd3dcbShadow;
	CB_SHADOW* m_pcbMappedShadow = nullptr;

	CB_SHADOW m_shadowData{};

	XMFLOAT4X4 m_shadowView{};

	UINT m_shadowMapSize = 2048;
	UINT m_shadowMapSrvIndex = UINT_MAX;

	float m_shadowOrthoHalfSize = 90.0f;
	float m_shadowNearZ = 1.0f;
	float m_shadowFarZ = 240.0f;

	float m_shadowConstantBias = 0.0008f;
	float m_shadowNormalBias = 0.0040f;
	float m_shadowIntensity = 1.0f;

	D3D12_VIEWPORT m_shadowViewport =
	{
		0.0f, 0.0f,
		2048.0f, 2048.0f,
		0.0f, 1.0f
	};

	D3D12_RECT m_shadowScissorRect =
	{
		0, 0, 2048, 2048
	};
};