#include "stdafx.h"
#include "DescriptorHeap.h"

#include <strsafe.h>

static void HeapDebugLog(const char* fmt, ...)
{
	char buffer[1024] = {};
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfA(buffer, _countof(buffer), fmt, args);
	va_end(args);
	OutputDebugStringA(buffer);
}

CDescriptorHeap::CDescriptorHeap()
{
}

CDescriptorHeap::~CDescriptorHeap()
{
	if (m_pd3dCbvSrvDescriptorHeap) 
		m_pd3dCbvSrvDescriptorHeap.Reset();
}

void CDescriptorHeap::CreateCbvSrvDescriptorHeaps(
	ID3D12Device* pd3dDevice,
	int nConstantBufferViews,
	int nShaderResourceViews)
{
	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
	d3dDescriptorHeapDesc.NumDescriptors = nConstantBufferViews + nShaderResourceViews;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	d3dDescriptorHeapDesc.NodeMask = 0;

	pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(m_pd3dCbvSrvDescriptorHeap.ReleaseAndGetAddressOf()));

	m_d3dCbvCPUDescriptorStartHandle = m_pd3dCbvSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_d3dCbvGPUDescriptorStartHandle = m_pd3dCbvSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	m_d3dSrvCPUDescriptorStartHandle.ptr = m_d3dCbvCPUDescriptorStartHandle.ptr + ( ::gnCbvSrvDescriptorIncrementSize * nConstantBufferViews );
	m_d3dSrvGPUDescriptorStartHandle.ptr = m_d3dCbvGPUDescriptorStartHandle.ptr + ( ::gnCbvSrvDescriptorIncrementSize * nConstantBufferViews );

	m_d3dCbvCPUDescriptorNextHandle = m_d3dCbvCPUDescriptorStartHandle;
	m_d3dCbvGPUDescriptorNextHandle = m_d3dCbvGPUDescriptorStartHandle;
	m_d3dSrvCPUDescriptorNextHandle = m_d3dSrvCPUDescriptorStartHandle;
	m_d3dSrvGPUDescriptorNextHandle = m_d3dSrvGPUDescriptorStartHandle;

	m_nCbvDescriptors = ( UINT ) nConstantBufferViews;
	m_nSrvDescriptors = ( UINT ) nShaderResourceViews;

	constexpr UINT kReservedLegacySrv = 6;
	m_nSrvAllocated = min(kReservedLegacySrv, m_nSrvDescriptors);
	m_nSrvBack = m_nSrvDescriptors;

	HeapDebugLog(
		"[Heap] Create heap cbv=%u srv=%u reservedFront=%u frontAllocated=%u backStart=%u\n",
		m_nCbvDescriptors,
		m_nSrvDescriptors,
		6u,
		m_nSrvAllocated,
		m_nSrvBack
	);
}

void CDescriptorHeap::CreateConstantBufferViews(ID3D12Device* pd3dDevice, int nConstantBufferViews, ID3D12Resource* pd3dConstantBuffers, UINT nStride)
{
	D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = pd3dConstantBuffers->GetGPUVirtualAddress();
	D3D12_CONSTANT_BUFFER_VIEW_DESC d3dCBVDesc;
	d3dCBVDesc.SizeInBytes = nStride;
	for (int j = 0; j < nConstantBufferViews; j++)
	{
		d3dCBVDesc.BufferLocation = d3dGpuVirtualAddress + (nStride * j);
		pd3dDevice->CreateConstantBufferView(&d3dCBVDesc, m_d3dCbvCPUDescriptorNextHandle);
		m_d3dCbvCPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
		m_d3dCbvGPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
	}
}

D3D12_GPU_DESCRIPTOR_HANDLE CDescriptorHeap::CreateConstantBufferView(ID3D12Device* pd3dDevice, ID3D12Resource* pd3dConstantBuffer, UINT nStride)
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC d3dCBVDesc;
	d3dCBVDesc.SizeInBytes = nStride;
	d3dCBVDesc.BufferLocation = pd3dConstantBuffer->GetGPUVirtualAddress();
	pd3dDevice->CreateConstantBufferView(&d3dCBVDesc, m_d3dCbvCPUDescriptorNextHandle);
	D3D12_GPU_DESCRIPTOR_HANDLE d3dCbvGPUDescriptorHandle = m_d3dCbvGPUDescriptorNextHandle;
	m_d3dCbvCPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
	m_d3dCbvGPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;

	return(d3dCbvGPUDescriptorHandle);
}

D3D12_GPU_DESCRIPTOR_HANDLE CDescriptorHeap::CreateConstantBufferView(ID3D12Device* pd3dDevice, D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress, UINT nStride)
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC d3dCBVDesc;
	d3dCBVDesc.SizeInBytes = nStride;
	d3dCBVDesc.BufferLocation = d3dGpuVirtualAddress;
	pd3dDevice->CreateConstantBufferView(&d3dCBVDesc, m_d3dCbvCPUDescriptorNextHandle);
	D3D12_GPU_DESCRIPTOR_HANDLE d3dCbvGPUDescriptorHandle = m_d3dCbvGPUDescriptorNextHandle;
	m_d3dCbvCPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
	m_d3dCbvGPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;

	return(d3dCbvGPUDescriptorHandle);
}

void CDescriptorHeap::CreateShaderResourceViews(
	ID3D12Device* pd3dDevice,
	CTexture* pTexture,
	UINT nDescriptorHeapIndex,
	UINT nRootParameterStartIndex)
{
	if ( !pd3dDevice || !pTexture )
	{
		OutputDebugStringA("[DescriptorHeap] ERROR: pd3dDevice or pTexture is null\n");
		return;
	}
	if ( !m_pd3dCbvSrvDescriptorHeap )
	{
		OutputDebugStringA("[DescriptorHeap] ERROR: m_pd3dCbvSrvDescriptorHeap is null\n");
		return;
	}
	if ( m_d3dSrvCPUDescriptorStartHandle.ptr == 0 || m_d3dSrvGPUDescriptorStartHandle.ptr == 0 )
	{
		OutputDebugStringA("[DescriptorHeap] ERROR: SRV start handle is null (heap not initialized?)\n");
		return;
	}

	HeapDebugLog(
		"[Heap] CreateSRVs textureBase=%u texCount=%d rootStart=%u\n",
		nDescriptorHeapIndex,
		pTexture->GetTextures(),
		nRootParameterStartIndex
	);

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_d3dSrvCPUDescriptorStartHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_d3dSrvGPUDescriptorStartHandle;

	cpuHandle.ptr += ( ::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex );
	gpuHandle.ptr += ( ::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex );

	int nTextures = pTexture->GetTextures();
	for ( int i = 0; i < nTextures; i++ )
	{
		ComPtr<ID3D12Resource> pShaderResource = pTexture->GetResource(i);
		if ( !pShaderResource )
		{
			char buf[256];
			sprintf_s(buf, "[DescriptorHeap] ERROR: Texture resource is null (i=%d)\n", i);
			OutputDebugStringA(buf);
			continue;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = pTexture->GetShaderResourceViewDesc(i);

		pd3dDevice->CreateShaderResourceView(
			pShaderResource.Get(),
			&srvDesc,
			cpuHandle);

		pTexture->SetGpuDescriptorHandle(i, gpuHandle);

		HeapDebugLog(
			"[Heap] SRV tex=%d res=%p format=%d viewDim=%d cpu=0x%p gpu=0x%llX srvIndex=%u\n",
			i,
			pShaderResource.Get(),
			srvDesc.Format,
			srvDesc.ViewDimension,
			reinterpret_cast< void* >( cpuHandle.ptr ),
			static_cast< unsigned long long >( gpuHandle.ptr ),
			nDescriptorHeapIndex + static_cast< UINT >( i )
		);

		cpuHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
		gpuHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
	}

	int nRootParameters = pTexture->GetRootParameters();
	for ( int i = 0; i < nRootParameters; i++ )
	{
		pTexture->SetRootParameterIndex(i, nRootParameterStartIndex + i);
	}
}

void CDescriptorHeap::CreateShaderResourceViews(
	ID3D12Device* pd3dDevice,
	int nResources,
	ID3D12Resource** ppd3dResources,
	DXGI_FORMAT* pdxgiSrvFormats)
{
	if ( !pd3dDevice || nResources <= 0 || !ppd3dResources || !pdxgiSrvFormats ) return;

	const UINT base = AllocateSrvRangeBack(( UINT ) nResources);
	if ( base == UINT_MAX ) return;

	HeapDebugLog(
		"[Heap] CreateSRVs(raw) count=%d base=%u\n",
		nResources,
		base
	);

	for ( int i = 0; i < nResources; i++ )
	{
		if ( !ppd3dResources[i] ) continue;

		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.Format = pdxgiSrvFormats[i];
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipLevels = 1;
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.PlaneSlice = 0;
		desc.Texture2D.ResourceMinLODClamp = 0.0f;

		pd3dDevice->CreateShaderResourceView(
			ppd3dResources[i],
			&desc,
			GetCPUSrvHandle(base + ( UINT ) i)
		);

		HeapDebugLog(
			"[Heap] CreateSRVs(raw) idx=%u res=%p format=%d viewDim=%d cpu=0x%p gpu=0x%llX\n",
			base + ( UINT ) i,
			ppd3dResources[i],
			desc.Format,
			desc.ViewDimension,
			reinterpret_cast< void* >( GetCPUSrvHandle(base + ( UINT ) i).ptr ),
			static_cast< unsigned long long >( GetGPUSrvHandle(base + ( UINT ) i).ptr )
		);
	}
}

D3D12_GPU_DESCRIPTOR_HANDLE CDescriptorHeap::CreateShaderResourceView(
	ID3D12Device* pd3dDevice,
	ID3D12Resource* pd3dResource,
	DXGI_FORMAT dxgiSrvFormat)
{
	D3D12_GPU_DESCRIPTOR_HANDLE nullH = { 0 };
	if ( !pd3dDevice || !pd3dResource )
		return nullH;

	const UINT idx = AllocateSrvRangeBack(1);
	if ( idx == UINT_MAX )
		return nullH;

	D3D12_RESOURCE_DESC resDesc = pd3dResource->GetDesc();

	HeapDebugLog(
		"[Heap] CreateSRV(single) res=%p resFormat=%d srvFormat=%d dimension=%d sampleCount=%u mipLevels=%u allocIdx=%u\n",
		pd3dResource,
		resDesc.Format,
		dxgiSrvFormat,
		resDesc.Dimension,
		resDesc.SampleDesc.Count,
		resDesc.MipLevels,
		idx
	);

	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Format = dxgiSrvFormat;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipLevels = 1;
	desc.Texture2D.MostDetailedMip = 0;
	desc.Texture2D.PlaneSlice = 0;
	desc.Texture2D.ResourceMinLODClamp = 0.0f;

	pd3dDevice->CreateShaderResourceView(
		pd3dResource,
		&desc,
		GetCPUSrvHandle(idx));

	HeapDebugLog(
		"[Heap] CreateSRV(single) done cpu=0x%p gpu=0x%llX viewDim=%d\n",
		reinterpret_cast< void* >( GetCPUSrvHandle(idx).ptr ),
		static_cast< unsigned long long >( GetGPUSrvHandle(idx).ptr ),
		desc.ViewDimension
	);

	return GetGPUSrvHandle(idx);
}


void CDescriptorHeap::CreateShaderResourceView(
	ID3D12Device* pd3dDevice,
	CTexture* pTexture,
	int nIndex,
	UINT nRootParameterStartIndex)
{
	if ( !pd3dDevice || !pTexture ) return;

	ComPtr<ID3D12Resource> res = pTexture->GetResource(nIndex);
	if ( !res ) return;

	if ( pTexture->GetGpuDescriptorHandle(nIndex).ptr ) return;

	const UINT idx = AllocateSrvRangeBack(1);
	if ( idx == UINT_MAX ) return;

	D3D12_SHADER_RESOURCE_VIEW_DESC desc = pTexture->GetShaderResourceViewDesc(nIndex);
	pd3dDevice->CreateShaderResourceView(res.Get(), &desc, GetCPUSrvHandle(idx));

	pTexture->SetGpuDescriptorHandle(nIndex, GetGPUSrvHandle(idx));
	pTexture->SetRootParameterIndex(nIndex, nRootParameterStartIndex + nIndex);

	HeapDebugLog(
		"[Heap] CreateSRV(tex,root) texIndex=%d srvIndex=%u res=%p format=%d viewDim=%d cpu=0x%p gpu=0x%llX root=%u\n",
		nIndex,
		idx,
		res.Get(),
		desc.Format,
		desc.ViewDimension,
		reinterpret_cast< void* >( GetCPUSrvHandle(idx).ptr ),
		static_cast< unsigned long long >( GetGPUSrvHandle(idx).ptr ),
		nRootParameterStartIndex + nIndex
	);
}


void CDescriptorHeap::CreateShaderResourceView(
	ID3D12Device* pd3dDevice,
	CTexture* pTexture,
	int nIndex)
{
	if ( !pd3dDevice || !pTexture ) return;

	ComPtr<ID3D12Resource> res = pTexture->GetResource(nIndex);
	if ( !res ) return;

	if ( pTexture->GetGpuDescriptorHandle(nIndex).ptr ) return;

	const UINT idx = AllocateSrvRangeBack(1);
	if ( idx == UINT_MAX ) return;

	D3D12_SHADER_RESOURCE_VIEW_DESC desc = pTexture->GetShaderResourceViewDesc(nIndex);
	pd3dDevice->CreateShaderResourceView(res.Get(), &desc, GetCPUSrvHandle(idx));

	pTexture->SetGpuDescriptorHandle(nIndex, GetGPUSrvHandle(idx));

	HeapDebugLog(
		"[Heap] CreateSRV(tex) texIndex=%d srvIndex=%u res=%p format=%d viewDim=%d cpu=0x%p gpu=0x%llX\n",
		nIndex,
		idx,
		res.Get(),
		desc.Format,
		desc.ViewDimension,
		reinterpret_cast< void* >( GetCPUSrvHandle(idx).ptr ),
		static_cast< unsigned long long >( GetGPUSrvHandle(idx).ptr )
	);
}


UINT CDescriptorHeap::AllocateSrvRange(UINT count)
{
	if ( count == 0 )
	{
		HeapDebugLog("[Heap] ERROR AllocateSrvRange failed count=%u front=%u back=%u srvCap=%u\n",
			count, m_nSrvAllocated, m_nSrvBack, m_nSrvDescriptors);
		return UINT_MAX;
	}

	if ( m_nSrvAllocated + count > m_nSrvBack )
	{
		HeapDebugLog("[Heap] ERROR AllocateSrvRange failed count=%u front=%u back=%u srvCap=%u\n",
			count, m_nSrvAllocated, m_nSrvBack, m_nSrvDescriptors);
		return UINT_MAX;
	}

	UINT base = m_nSrvAllocated;
	m_nSrvAllocated += count;

	HeapDebugLog(
		"[Heap] AllocateSrvRange front count=%u base=%u newFront=%u back=%u\n",
		count,
		base,
		m_nSrvAllocated,
		m_nSrvBack
	);

	return base;
}


D3D12_CPU_DESCRIPTOR_HANDLE CDescriptorHeap::GetCPUSrvHandle(UINT srvIndex) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE h = m_d3dSrvCPUDescriptorStartHandle;
	h.ptr += (::gnCbvSrvDescriptorIncrementSize * srvIndex);
	return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE CDescriptorHeap::GetGPUSrvHandle(UINT srvIndex) const
{
	D3D12_GPU_DESCRIPTOR_HANDLE h = m_d3dSrvGPUDescriptorStartHandle;
	h.ptr += (::gnCbvSrvDescriptorIncrementSize * srvIndex);
	return h;
}

void CDescriptorHeap::CreateShaderResourceViews(
	ID3D12Device* pd3dDevice,
	CTexture* pTexture,
	UINT nRootParameterStartIndex)
{
	if (!pTexture) return;

	UINT baseIndex = AllocateSrvRange((UINT)pTexture->GetTextures());
	if (baseIndex == UINT_MAX) return;

	// 텍스처가 SRV 슬롯 시작 인덱스를 기억
	pTexture->SetBaseSrvIndex(baseIndex);

	// 실제 SRV 생성은 기존 레거시 함수에 위임(내부적으로 절대 위치 baseIndex 사용)
	CreateShaderResourceViews(pd3dDevice, pTexture, baseIndex, nRootParameterStartIndex);

}

UINT CDescriptorHeap::AllocateSrvRangeBack(UINT count)
{
	if ( count == 0 )
	{
		HeapDebugLog("[Heap] ERROR AllocateSrvRangeBack failed count=%u front=%u back=%u srvCap=%u\n",
			count, m_nSrvAllocated, m_nSrvBack, m_nSrvDescriptors);
		return UINT_MAX;
	}

	if ( count > m_nSrvBack )
	{
		HeapDebugLog("[Heap] ERROR AllocateSrvRangeBack failed count=%u front=%u back=%u srvCap=%u\n",
			count, m_nSrvAllocated, m_nSrvBack, m_nSrvDescriptors);
		return UINT_MAX;
	}

	UINT newBase = m_nSrvBack - count;
	if ( newBase < m_nSrvAllocated )
	{
		HeapDebugLog("[Heap] ERROR AllocateSrvRangeBack failed count=%u front=%u back=%u srvCap=%u\n",
			count, m_nSrvAllocated, m_nSrvBack, m_nSrvDescriptors);
		return UINT_MAX;
	}

	m_nSrvBack = newBase;

	HeapDebugLog(
		"[Heap] AllocateSrvRangeBack count=%u base=%u front=%u newBack=%u\n",
		count,
		newBase,
		m_nSrvAllocated,
		m_nSrvBack
	);

	return newBase;
}

void CDescriptorHeap::CreateShaderResourceViewsOther(
	ID3D12Device* pd3dDevice,
	CTexture* pTexture,
	UINT nRootParameterStartIndex)
{
	if ( !pTexture ) return;

	UINT baseIndex = AllocateSrvRangeBack(( UINT ) pTexture->GetTextures());
	if ( baseIndex == UINT_MAX )
		return;

	HeapDebugLog(
		"[Heap] CreateSRVsOther base=%u texCount=%d rootStart=%u\n",
		baseIndex,
		pTexture->GetTextures(),
		nRootParameterStartIndex
	);

	pTexture->SetBaseSrvIndex(baseIndex);
	CreateShaderResourceViews(
		pd3dDevice,
		pTexture,
		baseIndex,
		nRootParameterStartIndex);
}
