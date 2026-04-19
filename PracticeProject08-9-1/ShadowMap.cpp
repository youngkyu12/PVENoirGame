#include "stdafx.h"
#include "ShadowMap.h"
#include "LightComponent.h"

ShadowMap::ShadowMap(ID3D12Device* device, CShader* shader, UINT width, UINT height)
{
	mDevice = device;
	mShader = shader;
	mWidth = width;
	mHeight = height;

	mViewport = { 0.0f, 0.0f, static_cast< float >( width ), static_cast< float >( height ), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast< int >( width ), static_cast< int >( height ) };

	CreateShadowPassCB();
	OnUpdate();
	UpdateShadowPassCB();
}

ShadowMap::~ShadowMap()
{
	if ( m_pd3dcbShadowPass && m_pcbMappedShadowPass )
	{
		m_pd3dcbShadowPass->Unmap(0, nullptr);
		m_pcbMappedShadowPass = nullptr;
	}
}

UINT ShadowMap::Width() const
{
	return mWidth;
}

UINT ShadowMap::Height() const
{
	return mHeight;
}

ID3D12Resource* ShadowMap::Resource()
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

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ShadowMap::Heap() const
{
	return mCbvHeap;
}

D3D12_VIEWPORT ShadowMap::Viewport() const
{
	return mViewport;
}

D3D12_RECT ShadowMap::ScissorRect() const
{
	return mScissorRect;
}

CB_SHADOW_PASS ShadowMap::GetConstants()
{
	CB_SHADOW_PASS shadowInfo;
	const XMMATRIX lightView = XMLoadFloat4x4(&mLightView);
	const XMMATRIX lightProj = XMLoadFloat4x4(&mLightProj);
	const XMMATRIX lightViewProj = XMMatrixMultiply(lightView, lightProj);

	shadowInfo.ViewProj = XMMatrixTranspose(lightViewProj);
	shadowInfo.ShadowTransform = mShadowTransform;
	shadowInfo.EyePosW = mLightPosW;
	return shadowInfo;
}

void ShadowMap::CreateShadowPassCB()
{
	if ( !mDevice )
		return;

	m_pShadow = std::make_unique<CB_SHADOW_PASS>();

	const UINT ncbElementBytes = ( sizeof(CB_SHADOW_PASS) + 255u ) & ~255u;
	m_pd3dcbShadowPass = CreateBufferResource(
		mDevice.Get(),
		nullptr,
		nullptr,
		ncbElementBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr);

	if ( m_pd3dcbShadowPass )
		m_pd3dcbShadowPass->Map(0, nullptr, reinterpret_cast< void** >( &m_pcbMappedShadowPass ));
}

void ShadowMap::UpdateShadowPassCB()
{
	if ( !m_pcbMappedShadowPass )
		return;

	if ( !m_pShadow )
		m_pShadow = std::make_unique<CB_SHADOW_PASS>();

	*m_pShadow = GetConstants();
	::memcpy(m_pcbMappedShadowPass, m_pShadow.get(), sizeof(CB_SHADOW_PASS));
}

void ShadowMap::BindShadowPassCB(ID3D12GraphicsCommandList* commandList) const
{
	if ( !commandList || !m_pd3dcbShadowPass )
		return;

	commandList->SetGraphicsRootConstantBufferView(
		ROOT_PARAMETER_SHADOW_PASS,
		m_pd3dcbShadowPass->GetGPUVirtualAddress());
}

void ShadowMap::OnCreate(D3D12_CPU_DESCRIPTOR_HANDLE dsvStart)
{
	mhCpuDsv = dsvStart;
	if ( ( mhCpuSrv.ptr != 0 ) && ( mhCpuDsv.ptr != 0 ) )
		BuildDescriptors();
}

void ShadowMap::OnUpdate()
{
	XMFLOAT3 targetPos = { 0.0f, 0.0f, 0.0f };
	if ( m_pTarget )
		targetPos = m_pTarget->GetPosition();

	XMFLOAT3 lightDir = { -0.577f, -0.577f, 0.577f };
	if ( m_pLight )
	{
		LIGHT lightData = {};
		m_pLight->Fill(lightData);
		lightDir = lightData.m_xmf3Direction;
	}

	XMVECTOR target = XMLoadFloat3(&targetPos);
	XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&lightDir));
	XMVECTOR lightPos = XMVectorSubtract(target, XMVectorScale(dir, 150.0f));
	const XMVECTOR upDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, target, upDir);
	XMMATRIX lightProj = XMMatrixOrthographicLH(220.0f, 220.0f, 1.0f, 800.0f);

	mLightNearZ = 1.0f;
	mLightFarZ = 800.0f;
	XMStoreFloat3(&mLightPosW, lightPos);
	XMStoreFloat4x4(&mLightView, lightView);
	XMStoreFloat4x4(&mLightProj, lightProj);

	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX shadowTransform = XMMatrixMultiply(XMMatrixMultiply(lightView, lightProj), T);
	XMStoreFloat4x4(&mShadowTransform, shadowTransform);
}

void ShadowMap::Render(const CGameTimer& /*gt*/, ID3D12GraphicsCommandList* commandList, std::vector<CGameObject*> components)
{
	if ( !commandList || !mShadowMap )
		return;

	OnUpdate();
	UpdateShadowPassCB();
	BindShadowPassCB(commandList);

	D3D12_RESOURCE_BARRIER toDepthWrite = CD3DX12_RESOURCE_BARRIER::Transition(
		mShadowMap.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_DEPTH_WRITE);
	commandList->ResourceBarrier(1, &toDepthWrite);

	commandList->RSSetViewports(1, &mViewport);
	commandList->RSSetScissorRects(1, &mScissorRect);
	commandList->ClearDepthStencilView(mhCpuDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	commandList->OMSetRenderTargets(0, nullptr, FALSE, &mhCpuDsv);

	if ( mShader )
		mShader->OnPrepareRender(commandList);

	for ( CGameObject* obj : components )
	{
		if ( !obj )
			continue;
		obj->Render(commandList, nullptr);
	}

	D3D12_RESOURCE_BARRIER toShaderRead = CD3DX12_RESOURCE_BARRIER::Transition(
		mShadowMap.Get(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_GENERIC_READ);
	commandList->ResourceBarrier(1, &toShaderRead);
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

void ShadowMap::OnResize(UINT newWidth, UINT newHeight)
{
	if ( ( mWidth != newWidth ) || ( mHeight != newHeight ) )
	{
		mWidth = newWidth;
		mHeight = newHeight;

		mViewport = { 0.0f, 0.0f, static_cast< float >( newWidth ), static_cast< float >( newHeight ), 0.0f, 1.0f };
		mScissorRect = { 0, 0, static_cast< int >( newWidth ), static_cast< int >( newHeight ) };

		BuildResource();

		if ( ( mhCpuSrv.ptr != 0 ) && ( mhCpuDsv.ptr != 0 ) )
			BuildDescriptors();
	}
}

void ShadowMap::BuildDescriptors()
{
	if ( !mShadowMap )
		return;

	if ( mhCpuSrv.ptr != 0 )
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.Texture2D.PlaneSlice = 0;
		mDevice->CreateShaderResourceView(mShadowMap.Get(), &srvDesc, mhCpuSrv);
	}

	if ( mhCpuDsv.ptr != 0 )
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Texture2D.MipSlice = 0;
		mDevice->CreateDepthStencilView(mShadowMap.Get(), &dsvDesc, mhCpuDsv);
	}
}

void ShadowMap::BuildResource()
{
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear = {};
	optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES d3dHeapProperties = {};
	::ZeroMemory(&d3dHeapProperties, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	d3dHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapProperties.CreationNodeMask = 1;
	d3dHeapProperties.VisibleNodeMask = 1;

	HRESULT hr = mDevice->CreateCommittedResource(
		&d3dHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&mShadowMap));
	assert(SUCCEEDED(hr));
}
