#include "stdafx.h"
#include "DescriptorHeap.h"

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
	d3dDescriptorHeapDesc.NumDescriptors = nConstantBufferViews + nShaderResourceViews; //CBVs + SRVs 
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	d3dDescriptorHeapDesc.NodeMask = 0;
	pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(m_pd3dCbvSrvDescriptorHeap.ReleaseAndGetAddressOf()));

	m_d3dCbvCPUDescriptorStartHandle = m_pd3dCbvSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_d3dCbvGPUDescriptorStartHandle = m_pd3dCbvSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	m_d3dSrvCPUDescriptorStartHandle.ptr = m_d3dCbvCPUDescriptorStartHandle.ptr + (::gnCbvSrvDescriptorIncrementSize * nConstantBufferViews);
	m_d3dSrvGPUDescriptorStartHandle.ptr = m_d3dCbvGPUDescriptorStartHandle.ptr + (::gnCbvSrvDescriptorIncrementSize * nConstantBufferViews);

	m_d3dCbvCPUDescriptorNextHandle = m_d3dCbvCPUDescriptorStartHandle;
	m_d3dCbvGPUDescriptorNextHandle = m_d3dCbvGPUDescriptorStartHandle;
	m_d3dSrvCPUDescriptorNextHandle = m_d3dSrvCPUDescriptorStartHandle;
	m_d3dSrvGPUDescriptorNextHandle = m_d3dSrvGPUDescriptorStartHandle;

	m_nCbvDescriptors = (UINT)nConstantBufferViews;
	m_nSrvDescriptors = (UINT)nShaderResourceViews;
	m_nSrvAllocated = 0;

	m_nSrvAllocated = 0;            // 앞쪽(머티리얼) 커서
	m_nSrvBack = m_nSrvDescriptors; // 뒤쪽(기타) 커서 (exclusive)
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
	if (!pd3dDevice || !pTexture)
	{
		OutputDebugStringA("[DescriptorHeap] ERROR: pd3dDevice or pTexture is null\n");
		return;
	}
	if (!m_pd3dCbvSrvDescriptorHeap)
	{
		OutputDebugStringA("[DescriptorHeap] ERROR: m_pd3dCbvSrvDescriptorHeap is null\n");
		return;
	}
	if (m_d3dSrvCPUDescriptorStartHandle.ptr == 0 || m_d3dSrvGPUDescriptorStartHandle.ptr == 0)
	{
		OutputDebugStringA("[DescriptorHeap] ERROR: SRV start handle is null (heap not initialized?)\n");
		return;
	}

	// 핵심: NextHandle을 절대 누적 이동시키지 말 것.
	// StartHandle 기준으로 로컬 핸들을 계산해서 사용한다.
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_d3dSrvCPUDescriptorStartHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_d3dSrvGPUDescriptorStartHandle;

	cpuHandle.ptr += (::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);
	gpuHandle.ptr += (::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);

	int nTextures = pTexture->GetTextures();
	for (int i = 0; i < nTextures; i++)
	{
		ComPtr<ID3D12Resource> pShaderResource = pTexture->GetResource(i);
		if (!pShaderResource)
		{
			char buf[256];
			sprintf_s(buf, "[DescriptorHeap] ERROR: Texture resource is null (i=%d)\n", i);
			OutputDebugStringA(buf);
			// 로컬 핸들은 그대로 증가시키지 않는 편이 안전(슬롯 낭비 방지)
			continue;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = pTexture->GetShaderResourceViewDesc(i);

		pd3dDevice->CreateShaderResourceView(
			pShaderResource.Get(),
			&srvDesc,
			cpuHandle);

		// 텍스처에 GPU 핸들 저장 (바인딩 시 사용)
		pTexture->SetGpuDescriptorHandle(i, gpuHandle);

		// 다음 슬롯 (로컬 핸들만 이동)
		cpuHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
		gpuHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
	}

	int nRootParameters = pTexture->GetRootParameters();
	for (int i = 0; i < nRootParameters; i++)
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
	if (!pd3dDevice || nResources <= 0 || !ppd3dResources || !pdxgiSrvFormats) return;

	const UINT base = AllocateSrvRangeBack((UINT)nResources);
	if (base == UINT_MAX) return;

	for (int i = 0; i < nResources; i++)
	{
		if (!ppd3dResources[i]) continue;

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
			GetCPUSrvHandle(base + (UINT)i)
		);
	}
}

D3D12_GPU_DESCRIPTOR_HANDLE CDescriptorHeap::CreateShaderResourceView(
	ID3D12Device* pd3dDevice,
	ID3D12Resource* pd3dResource,
	DXGI_FORMAT dxgiSrvFormat)
{
	D3D12_GPU_DESCRIPTOR_HANDLE nullH = { 0 };
	if (!pd3dDevice || !pd3dResource) return nullH;

	const UINT idx = AllocateSrvRangeBack(1);
	if (idx == UINT_MAX) return nullH;

	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Format = dxgiSrvFormat;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipLevels = 1;
	desc.Texture2D.MostDetailedMip = 0;
	desc.Texture2D.PlaneSlice = 0;
	desc.Texture2D.ResourceMinLODClamp = 0.0f;

	pd3dDevice->CreateShaderResourceView(pd3dResource, &desc, GetCPUSrvHandle(idx));
	return GetGPUSrvHandle(idx);
}


void CDescriptorHeap::CreateShaderResourceView(
	ID3D12Device* pd3dDevice,
	CTexture* pTexture,
	int nIndex,
	UINT nRootParameterStartIndex)
{
	if (!pd3dDevice || !pTexture) return;

	ComPtr<ID3D12Resource> res = pTexture->GetResource(nIndex);
	if (!res) return;

	// 이미 핸들 있으면 스킵(기존 로직 유지)
	if (pTexture->GetGpuDescriptorHandle(nIndex).ptr) return;

	const UINT idx = AllocateSrvRangeBack(1);
	if (idx == UINT_MAX) return;

	D3D12_SHADER_RESOURCE_VIEW_DESC desc = pTexture->GetShaderResourceViewDesc(nIndex);
	pd3dDevice->CreateShaderResourceView(res.Get(), &desc, GetCPUSrvHandle(idx));

	pTexture->SetGpuDescriptorHandle(nIndex, GetGPUSrvHandle(idx));
	pTexture->SetRootParameterIndex(nIndex, nRootParameterStartIndex + nIndex);
}


void CDescriptorHeap::CreateShaderResourceView(
	ID3D12Device* pd3dDevice,
	CTexture* pTexture,
	int nIndex)
{
	if (!pd3dDevice || !pTexture) return;

	ComPtr<ID3D12Resource> res = pTexture->GetResource(nIndex);
	if (!res) return;

	if (pTexture->GetGpuDescriptorHandle(nIndex).ptr) return;

	const UINT idx = AllocateSrvRangeBack(1);
	if (idx == UINT_MAX) return;

	D3D12_SHADER_RESOURCE_VIEW_DESC desc = pTexture->GetShaderResourceViewDesc(nIndex);
	pd3dDevice->CreateShaderResourceView(res.Get(), &desc, GetCPUSrvHandle(idx));

	pTexture->SetGpuDescriptorHandle(nIndex, GetGPUSrvHandle(idx));
}


UINT CDescriptorHeap::AllocateSrvRange(UINT count)
{
	if (count == 0) return UINT_MAX;
	if (m_nSrvAllocated + count > m_nSrvBack) return UINT_MAX; // <-- 핵심: 뒤쪽과 충돌 방지
	UINT base = m_nSrvAllocated;
	m_nSrvAllocated += count;
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
    if (count == 0) return UINT_MAX;
    if (count > m_nSrvBack) return UINT_MAX;

    UINT newBase = m_nSrvBack - count;
    if (newBase < m_nSrvAllocated) return UINT_MAX; // <-- 앞쪽(머티리얼)과 충돌 방지

    m_nSrvBack = newBase;
    return newBase;
}

void CDescriptorHeap::CreateShaderResourceViewsOther(
    ID3D12Device* pd3dDevice,
    CTexture* pTexture,
    UINT nRootParameterStartIndex)
{
    if (!pTexture) return;

    UINT baseIndex = AllocateSrvRangeBack((UINT)pTexture->GetTextures());
    if (baseIndex == UINT_MAX) 
		return;

    pTexture->SetBaseSrvIndex(baseIndex);
    CreateShaderResourceViews(pd3dDevice, pTexture, baseIndex, nRootParameterStartIndex);
}
