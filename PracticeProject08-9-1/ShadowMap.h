//-----------------------------------------------------------------------------
// File: ShadowMap.h
//-----------------------------------------------------------------------------

#pragma once

#include "MathHelper.h"
#include "d3dx12.h"

#include <DirectXMath.h>
#include <wrl.h>

struct CB_SHADOWMAP_INFO
{
	DirectX::XMMATRIX ViewProj = DirectX::XMMatrixIdentity();
	DirectX::XMFLOAT4X4 ShadowTransform = Matrix4x4::Identity();
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float _pad = 0.0f;
};

class ShadowMap
{
public:
	ShadowMap(ID3D12Device* device, UINT width, UINT height);

	ShadowMap(const ShadowMap& rhs) = delete;
	ShadowMap& operator=(const ShadowMap& rhs) = delete;
	~ShadowMap() = default;

public:
	UINT Width() const;
	UINT Height() const;

	ID3D12Resource* Resource() const;

	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;

	D3D12_VIEWPORT Viewport() const;
	D3D12_RECT ScissorRect() const;

public:
	void BuildDescriptors(
		D3D12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		D3D12_CPU_DESCRIPTOR_HANDLE hCpuDsv
	);

	void OnResize(UINT newWidth, UINT newHeight);

public:
	void SetLightDirection(const DirectX::XMFLOAT3& lightDirection);
	void SetShadowFocus(const DirectX::XMFLOAT3& focusCenter, float focusRadius);

	void OnUpdate();
	CB_SHADOWMAP_INFO GetConstants() const;

public:
	const DirectX::XMFLOAT4X4& GetLightView() const { return mLightView; }
	const DirectX::XMFLOAT4X4& GetLightProj() const { return mLightProj; }
	const DirectX::XMFLOAT4X4& GetShadowTransform() const { return mShadowTransform; }

	const DirectX::XMFLOAT3& GetLightPosition() const { return mLightPosW; }
	float GetLightNearZ() const { return mLightNearZ; }
	float GetLightFarZ() const { return mLightFarZ; }

private:
	void BuildDescriptors();
	void BuildResource();
	void UpdateViewportAndScissorRect();

private:
	ComPtr<ID3D12Device> mDevice;

	D3D12_VIEWPORT mViewport = {};
	D3D12_RECT mScissorRect = {};

	UINT mWidth = 0;
	UINT mHeight = 0;

	DXGI_FORMAT mResourceFormat = DXGI_FORMAT_R24G8_TYPELESS;
	DXGI_FORMAT mSrvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	DXGI_FORMAT mDsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	D3D12_CPU_DESCRIPTOR_HANDLE mhCpuSrv = {};
	D3D12_GPU_DESCRIPTOR_HANDLE mhGpuSrv = {};
	D3D12_CPU_DESCRIPTOR_HANDLE mhCpuDsv = {};

	ComPtr<ID3D12Resource> mShadowMap = nullptr;

	DirectX::XMFLOAT3 mLightDirection = { 1.0f, -1.0f, 0.3f };
	DirectX::XMFLOAT3 mShadowFocusCenter = { 0.0f, 0.0f, 0.0f };
	float mShadowFocusRadius = 500.0f;

	DirectX::XMFLOAT3 mLightPosW = { 0.0f, 0.0f, 0.0f };
	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;

	DirectX::XMFLOAT4X4 mLightView = Matrix4x4::Identity();
	DirectX::XMFLOAT4X4 mLightProj = Matrix4x4::Identity();
	DirectX::XMFLOAT4X4 mShadowTransform = Matrix4x4::Identity();
};