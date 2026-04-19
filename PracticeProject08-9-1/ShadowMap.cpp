//-----------------------------------------------------------------------------
// File: ShadowMap.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "ShadowMap.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
	static constexpr float kMinShadowFocusRadius = 1.0f;
	static constexpr float kLightDistanceScale = 2.0f;
	static constexpr float kNearFarRadiusScale = 2.0f;
}

ShadowMap::ShadowMap(ID3D12Device* device, UINT width, UINT height)
	: mDevice(device)
	, mWidth(width)
	, mHeight(height)
{
	UpdateViewportAndScissorRect();
	BuildResource();
	OnUpdate();
}

UINT ShadowMap::Width() const
{
	return mWidth;
}

UINT ShadowMap::Height() const
{
	return mHeight;
}

ID3D12Resource* ShadowMap::Resource() const
{
	return mShadowMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowMap::Srv() const
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(mhGpuSrv);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMap::Dsv() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mhCpuDsv);
}

D3D12_VIEWPORT ShadowMap::Viewport() const
{
	return mViewport;
}

D3D12_RECT ShadowMap::ScissorRect() const
{
	return mScissorRect;
}

void ShadowMap::BuildDescriptors(
	D3D12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
	D3D12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;
	mhCpuDsv = hCpuDsv;

	BuildDescriptors();
}

void ShadowMap::BuildDescriptors()
{
	if ( !mDevice ) return;
	if ( !mShadowMap ) return;
	if ( mhCpuSrv.ptr == 0 ) return;
	if ( mhCpuDsv.ptr == 0 ) return;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mSrvFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	mDevice->CreateShaderResourceView(
		mShadowMap.Get(),
		&srvDesc,
		mhCpuSrv
	);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDsvFormat;
	dsvDesc.Texture2D.MipSlice = 0;

	mDevice->CreateDepthStencilView(
		mShadowMap.Get(),
		&dsvDesc,
		mhCpuDsv
	);
}

void ShadowMap::BuildResource()
{
	if ( !mDevice ) return;

	D3D12_CLEAR_VALUE optClear = {};
	optClear.Format = mDsvFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		mResourceFormat,
		static_cast< UINT64 >( mWidth ),
		static_cast< UINT >( mHeight ),
		1,
		1,
		1,
		0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	);

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	HRESULT hr = mDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mShadowMap.ReleaseAndGetAddressOf())
	);

	if ( FAILED(hr) )
	{
		OutputDebugStringA("[ShadowMap] CreateCommittedResource FAILED\n");
		return;
	}
}

void ShadowMap::UpdateViewportAndScissorRect()
{
	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width = static_cast< float >( mWidth );
	mViewport.Height = static_cast< float >( mHeight );
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;

	mScissorRect.left = 0;
	mScissorRect.top = 0;
	mScissorRect.right = static_cast< LONG >( mWidth );
	mScissorRect.bottom = static_cast< LONG >( mHeight );
}

void ShadowMap::OnResize(UINT newWidth, UINT newHeight)
{
	if ( ( mWidth == newWidth ) && ( mHeight == newHeight ) )
		return;

	if ( newWidth == 0 || newHeight == 0 )
		return;

	mWidth = newWidth;
	mHeight = newHeight;

	UpdateViewportAndScissorRect();
	BuildResource();
	BuildDescriptors();
}

void ShadowMap::SetLightDirection(const XMFLOAT3& lightDirection)
{
	mLightDirection = lightDirection;
}

void ShadowMap::SetShadowFocus(const XMFLOAT3& focusCenter, float focusRadius)
{
	mShadowFocusCenter = focusCenter;
	mShadowFocusRadius = ( std::max ) ( focusRadius, kMinShadowFocusRadius );
}

void ShadowMap::OnUpdate()
{
	const XMVECTOR defaultDir = XMVectorSet(1.0f, -1.0f, 0.3f, 0.0f);

	XMVECTOR lightDir = XMLoadFloat3(&mLightDirection);
	if ( XMVector3Less(XMVector3LengthSq(lightDir), XMVectorReplicate(1.0e-6f)) )
	{
		lightDir = defaultDir;
	}

	lightDir = XMVector3Normalize(lightDir);

	const float sphereRadius = ( std::max ) ( mShadowFocusRadius, kMinShadowFocusRadius );
	const XMVECTOR focusCenter = XMLoadFloat3(&mShadowFocusCenter);

	const XMVECTOR lightPos =
		focusCenter - ( lightDir * ( sphereRadius * kLightDistanceScale ) );

	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const float dirDotUp = std::fabs(XMVectorGetX(XMVector3Dot(lightDir, up)));
	if ( dirDotUp > 0.99f )
	{
		up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	}

	const XMMATRIX lightView = XMMatrixLookAtLH(lightPos, focusCenter, up);

	XMStoreFloat3(&mLightPosW, lightPos);

	const XMVECTOR centerLS = XMVector3TransformCoord(focusCenter, lightView);

	const float left = XMVectorGetX(centerLS) - sphereRadius;
	const float right = XMVectorGetX(centerLS) + sphereRadius;
	const float bottom = XMVectorGetY(centerLS) - sphereRadius;
	const float top = XMVectorGetY(centerLS) + sphereRadius;

	mLightNearZ = ( std::max ) ( 0.0f, XMVectorGetZ(centerLS) - sphereRadius * kNearFarRadiusScale );
	mLightFarZ = XMVectorGetZ(centerLS) + sphereRadius * kNearFarRadiusScale;

	if ( mLightFarZ <= mLightNearZ + 1.0f )
	{
		mLightFarZ = mLightNearZ + 1.0f;
	}

	const XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(
		left,
		right,
		bottom,
		top,
		mLightNearZ,
		mLightFarZ
	);

	const XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	);

	const XMMATRIX shadowTransform = lightView * lightProj * T;

	XMStoreFloat4x4(&mLightView, lightView);
	XMStoreFloat4x4(&mLightProj, lightProj);
	XMStoreFloat4x4(&mShadowTransform, shadowTransform);
}

CB_SHADOWMAP_INFO ShadowMap::GetConstants() const
{
	CB_SHADOWMAP_INFO out = {};

	const XMMATRIX V = XMLoadFloat4x4(&mLightView);
	const XMMATRIX P = XMLoadFloat4x4(&mLightProj);
	const XMMATRIX S = XMLoadFloat4x4(&mShadowTransform);

	out.ViewProj = XMMatrixTranspose(V * P);
	XMStoreFloat4x4(&out.ShadowTransform, XMMatrixTranspose(S));
	out.EyePosW = mLightPosW;
	out._pad = 0.0f;

	return out;
}