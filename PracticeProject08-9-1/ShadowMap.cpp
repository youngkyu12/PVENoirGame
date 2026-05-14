//-----------------------------------------------------------------------------
// File: ShadowMap.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "ShadowMap.h"

#include "DescriptorHeap.h"
#include "Object.h"
#include "Scene.h"

CShadowMapSystem::~CShadowMapSystem()
{
	ReleaseResources();
}

void CShadowMapSystem::SetFrameResourceIndex(UINT frameResourceIndex)
{
	m_nFrameResourceIndex = frameResourceIndex % kFrameResourceCount;
}

void CShadowMapSystem::BuildResources(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	CDescriptorHeap* descriptorHeap)
{
	UNREFERENCED_PARAMETER(cmd);

	if ( !dev )
		return;

	if ( !descriptorHeap )
		return;

	m_shadowViewport = {
		0.0f,
		0.0f,
		static_cast< float >( m_shadowMapSize ),
		static_cast< float >( m_shadowMapSize ),
		0.0f,
		1.0f
	};

	m_shadowScissorRect = {
		0,
		0,
		static_cast< LONG >( m_shadowMapSize ),
		static_cast< LONG >( m_shadowMapSize )
	};

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = dev->CreateDescriptorHeap(
		&dsvHeapDesc,
		IID_PPV_ARGS(m_pd3dShadowDsvHeap.ReleaseAndGetAddressOf())
	);

	if ( FAILED(hr) )
	{
		OutputDebugStringA("[Shadow] Create DSV heap failed.\n");
		return;
	}

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	m_pd3dShadowMap.Attach(
		::CreateTexture2DResource(
			dev,
			m_shadowMapSize,
			m_shadowMapSize,
			1,
			1,
			DXGI_FORMAT_R24G8_TYPELESS,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue
		)
	);

	if ( !m_pd3dShadowMap )
	{
		OutputDebugStringA("[Shadow] Create shadow map resource failed.\n");
		return;
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	dev->CreateDepthStencilView(
		m_pd3dShadowMap.Get(),
		&dsvDesc,
		m_pd3dShadowDsvHeap->GetCPUDescriptorHandleForHeapStart()
	);

	if ( m_shadowMapSrvIndex == UINT_MAX )
		m_shadowMapSrvIndex = descriptorHeap->AllocateSrvRangeBack(1);

	if ( m_shadowMapSrvIndex == UINT_MAX )
	{
		OutputDebugStringA("[Shadow] Allocate SRV slot failed.\n");
		return;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	dev->CreateShaderResourceView(
		m_pd3dShadowMap.Get(),
		&srvDesc,
		descriptorHeap->GetCPUSrvHandle(m_shadowMapSrvIndex)
	);

	const UINT cbBytes = ( sizeof(CB_SHADOW) + 255u ) & ~255u;

	for ( UINT i = 0; i < kFrameResourceCount; ++i )
	{
		m_pd3dcbShadow[i] = ::CreateBufferResource(
			dev,
			nullptr,
			nullptr,
			cbBytes,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			nullptr
		);

		if ( m_pd3dcbShadow[i] )
		{
			m_pd3dcbShadow[i]->Map(
				0,
				nullptr,
				reinterpret_cast< void** >( &m_pcbMappedShadow[i] )
			);
		}
	}

	m_nFrameResourceIndex = 0;

	m_shadowData.shadowParams0 = XMFLOAT4(
		static_cast< float >( m_shadowMapSize ),
		m_shadowConstantBias,
		m_shadowNormalBias,
		m_shadowIntensity
	);

	m_shadowData.shadowParams1 = XMUINT4(
		m_shadowMapSrvIndex,
		0u,
		0u,
		0u
	);

	UploadConstantBuffer();
}

void CShadowMapSystem::ReleaseResources()
{
	for ( UINT i = 0; i < kFrameResourceCount; ++i )
	{
		if ( m_pd3dcbShadow[i] )
		{
			if ( m_pcbMappedShadow[i] )
			{
				m_pd3dcbShadow[i]->Unmap(0, nullptr);
				m_pcbMappedShadow[i] = nullptr;
			}

			m_pd3dcbShadow[i].Reset();
		}

		m_pcbMappedShadow[i] = nullptr;
	}

	if ( m_pd3dShadowMap )
		m_pd3dShadowMap.Reset();

	if ( m_pd3dShadowDsvHeap )
		m_pd3dShadowDsvHeap.Reset();

	m_shadowMapSrvIndex = UINT_MAX;
	m_shadowData = CB_SHADOW{};
	m_shadowView = XMFLOAT4X4{};
	m_nFrameResourceIndex = 0;
}

void CShadowMapSystem::BindConstantBuffer(ID3D12GraphicsCommandList* cmd) const
{
	if ( !cmd )
		return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	if ( !m_pd3dcbShadow[frameIndex] )
		return;

	cmd->SetGraphicsRootConstantBufferView(
		ROOT_PARAMETER_SHADOW,
		m_pd3dcbShadow[frameIndex]->GetGPUVirtualAddress()
	);
}

void CShadowMapSystem::UpdateData(
	CGameObject* focusObject,
	CGameObject* directionalLightObject)
{
	m_shadowData.shadowParams1 = XMUINT4(UINT_MAX, 0u, 0u, 0u);

	if ( !focusObject )
		return;

	if ( !directionalLightObject )
		return;

	if ( m_shadowMapSrvIndex == UINT_MAX )
		return;

	auto* lightTr =
		directionalLightObject->GetComponent<CTransformComponent>();

	if ( !lightTr )
		return;

	XMFLOAT3 lightDir = lightTr->GetLook();
	XMVECTOR lightDirV = XMLoadFloat3(&lightDir);

	if ( XMVectorGetX(XMVector3LengthSq(lightDirV)) < 1.0e-6f )
		lightDirV = XMVectorSet(1.0f, -1.0f, 0.3f, 0.0f);

	lightDirV = XMVector3Normalize(lightDirV);

	XMFLOAT3 center = focusObject->GetPosition();
	center.y += 10.0f;

	XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f);

	if ( fabsf(XMVectorGetX(XMVector3Dot(lightDirV, XMLoadFloat3(&up)))) > 0.98f )
		up = XMFLOAT3(0.0f, 0.0f, 1.0f);

	XMFLOAT3 eye{};
	XMStoreFloat3(
		&eye,
		XMLoadFloat3(&center) - ( lightDirV * ( m_shadowFarZ * 0.5f ) )
	);

	const XMMATRIX view =
		XMMatrixLookAtLH(
			XMLoadFloat3(&eye),
			XMLoadFloat3(&center),
			XMLoadFloat3(&up)
		);

	const XMMATRIX proj =
		XMMatrixOrthographicLH(
			m_shadowOrthoHalfSize * 2.0f,
			m_shadowOrthoHalfSize * 2.0f,
			m_shadowNearZ,
			m_shadowFarZ
		);

	const XMMATRIX tex =
		XMMATRIX(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f
		);

	const XMMATRIX shadowViewProj = view * proj;
	const XMMATRIX shadowTransform = shadowViewProj * tex;

	XMStoreFloat4x4(&m_shadowView, view);

	XMStoreFloat4x4(
		&m_shadowData.shadowViewProj,
		XMMatrixTranspose(shadowViewProj)
	);

	XMStoreFloat4x4(
		&m_shadowData.shadowTransform,
		XMMatrixTranspose(shadowTransform)
	);

	XMFLOAT3 lightDirOut{};
	XMStoreFloat3(&lightDirOut, lightDirV);

	m_shadowData.shadowLightPos =
		XMFLOAT4(lightDirOut.x, lightDirOut.y, lightDirOut.z, 0.0f);

	m_shadowData.shadowParams0 = XMFLOAT4(
		static_cast< float >( m_shadowMapSize ),
		m_shadowConstantBias,
		m_shadowNormalBias,
		m_shadowIntensity
	);

	m_shadowData.shadowParams1 = XMUINT4(
		m_shadowMapSrvIndex,
		1u,
		0u,
		0u
	);
}

void CShadowMapSystem::UploadConstantBuffer()
{
	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	if ( !m_pcbMappedShadow[frameIndex] )
		return;

	memcpy(m_pcbMappedShadow[frameIndex], &m_shadowData, sizeof(CB_SHADOW));
}

bool CShadowMapSystem::BeginRender(
	ID3D12GraphicsCommandList* cmd,
	ID3D12RootSignature* rootSignature,
	CDescriptorHeap* descriptorHeap,
	D3D12_GPU_VIRTUAL_ADDRESS materialCbGpuAddress)
{
	if ( !cmd ) return false;
	if ( !rootSignature ) return false;
	if ( !descriptorHeap ) return false;
	if ( !IsReady() ) return false;

	::SynchronizeResourceTransition(
		cmd,
		m_pd3dShadowMap.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE
	);

	cmd->SetGraphicsRootSignature(rootSignature);

	if ( descriptorHeap->m_pd3dCbvSrvDescriptorHeap )
	{
		cmd->SetDescriptorHeaps(
			1,
			descriptorHeap->m_pd3dCbvSrvDescriptorHeap.GetAddressOf()
		);

		cmd->SetGraphicsRootDescriptorTable(
			ROOT_PARAMETER_GLOBAL_SRV,
			descriptorHeap->GetGPUSrvDescriptorStartHandle()
		);
	}

	cmd->RSSetViewports(1, &m_shadowViewport);
	cmd->RSSetScissorRects(1, &m_shadowScissorRect);

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
		m_pd3dShadowDsvHeap->GetCPUDescriptorHandleForHeapStart();

	cmd->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);

	cmd->ClearDepthStencilView(
		dsvHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f,
		0,
		0,
		nullptr
	);

	if ( materialCbGpuAddress != 0 )
	{
		cmd->SetGraphicsRootConstantBufferView(
			ROOT_PARAMETER_MATERIAL,
			materialCbGpuAddress
		);
	}

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	if ( !m_pd3dcbShadow[frameIndex] )
		return false;

	cmd->SetGraphicsRootConstantBufferView(
		ROOT_PARAMETER_SHADOW,
		m_pd3dcbShadow[frameIndex]->GetGPUVirtualAddress()
	);

	return true;
}

void CShadowMapSystem::EndRender(ID3D12GraphicsCommandList* cmd)
{
	if ( !cmd ) return;
	if ( !m_pd3dShadowMap ) return;

	::SynchronizeResourceTransition(
		cmd,
		m_pd3dShadowMap.Get(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
}

bool CShadowMapSystem::IsWorldOOBBInsideShadowBox(
	const BoundingOrientedBox& box) const
{
	XMFLOAT3 corners[BoundingOrientedBox::CORNER_COUNT];
	box.GetCorners(corners);

	const XMMATRIX shadowView = XMLoadFloat4x4(&m_shadowView);

	XMFLOAT3 minV(FLT_MAX, FLT_MAX, FLT_MAX);
	XMFLOAT3 maxV(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for ( const XMFLOAT3& corner : corners )
	{
		XMVECTOR p = XMLoadFloat3(&corner);
		p = XMVector3TransformCoord(p, shadowView);

		XMFLOAT3 v{};
		XMStoreFloat3(&v, p);

		minV.x = ( minV.x < v.x ) ? minV.x : v.x;
		minV.y = ( minV.y < v.y ) ? minV.y : v.y;
		minV.z = ( minV.z < v.z ) ? minV.z : v.z;

		maxV.x = ( maxV.x > v.x ) ? maxV.x : v.x;
		maxV.y = ( maxV.y > v.y ) ? maxV.y : v.y;
		maxV.z = ( maxV.z > v.z ) ? maxV.z : v.z;

	}

	const float half = m_shadowOrthoHalfSize;

	if ( maxV.x < -half || minV.x > half )
		return false;

	if ( maxV.y < -half || minV.y > half )
		return false;

	if ( maxV.z < m_shadowNearZ || minV.z > m_shadowFarZ )
		return false;

	return true;
}