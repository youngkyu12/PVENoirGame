//-----------------------------------------------------------------------------
// File: CTexture.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Texture.h"

CTexture::CTexture(int nTextures, UINT nTextureType, int nSamplers, int nRootParameters)
{
	m_nTextureType = nTextureType;

	m_nTextures = nTextures;
	if (m_nTextures > 0)
	{
		m_ppd3dTextures.resize(m_nTextures);
		m_ppd3dTextureUploadBuffers.resize(m_nTextures);
		m_pd3dSrvGpuDescriptorHandles = make_unique<D3D12_GPU_DESCRIPTOR_HANDLE[]>(m_nTextures);
		m_pnResourceTypes = make_unique<UINT[]>(m_nTextures);
		m_pdxgiBufferFormats = make_unique<DXGI_FORMAT[]>(m_nTextures);
		m_pnBufferElements = make_unique<int[]>(m_nTextures);
	}

	m_nRootParameters = nRootParameters;
	if (m_nRootParameters > 0) 
	{
		m_pnRootParameterIndices = make_unique<int[]>(m_nRootParameters);
		for (int i = 0; i < m_nRootParameters; i++)
			m_pnRootParameterIndices[i] = -1;
	}

	m_nSamplers = nSamplers;
	if (m_nSamplers > 0)
		m_pd3dSamplerGpuDescriptorHandles = make_unique<D3D12_GPU_DESCRIPTOR_HANDLE[]>(m_nSamplers);
}

CTexture::~CTexture()
{
	if (!m_ppd3dTextures.empty())
	{
		for (int i = 0; i < m_nTextures; i++)
			if (m_ppd3dTextures[i])
				m_ppd3dTextures[i].Reset();
	}
}

void CTexture::SetRootParameterIndex(int nIndex, UINT nRootParameterIndex)
{
	m_pnRootParameterIndices[nIndex] = nRootParameterIndex;
}

void CTexture::SetGpuDescriptorHandle(int nIndex, D3D12_GPU_DESCRIPTOR_HANDLE d3dSrvGpuDescriptorHandle)
{
	m_pd3dSrvGpuDescriptorHandles[nIndex] = d3dSrvGpuDescriptorHandle;
}

void CTexture::SetSampler(int nIndex, D3D12_GPU_DESCRIPTOR_HANDLE d3dSamplerGpuDescriptorHandle)
{
	m_pd3dSamplerGpuDescriptorHandles[nIndex] = d3dSamplerGpuDescriptorHandle;
}

void CTexture::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	/*
	//글로벌 SRV 테이블 사용 중이면 per-texture 바인딩 금지
	if (m_baseSrvIndex != UINT_MAX) return;

	if (m_nRootParameters > 0)
	{
		for (int i = 0; i < m_nRootParameters; i++)
		{
			pd3dCommandList->SetGraphicsRootDescriptorTable(
				m_pnRootParameterIndices[i],
				m_pd3dSrvGpuDescriptorHandles[i]
			);
		}
	}
	*/
}

void CTexture::UpdateShaderVariable(ID3D12GraphicsCommandList* pd3dCommandList, int nParameterIndex, int nTextureIndex)
{
	//글로벌 SRV 테이블 사용 중이면 per-texture 바인딩 금지
	if (m_baseSrvIndex != UINT_MAX) return;

	pd3dCommandList->SetGraphicsRootDescriptorTable(
		m_pnRootParameterIndices[nParameterIndex],
		m_pd3dSrvGpuDescriptorHandles[nTextureIndex]
	);
}

void CTexture::ReleaseShaderVariables()
{
}

void CTexture::ReleaseUploadBuffers()
{
	if (!m_ppd3dTextureUploadBuffers.empty())
	{
		for (int i = 0; i < m_nTextures; i++)
			if (m_ppd3dTextureUploadBuffers[i])
				m_ppd3dTextureUploadBuffers[i].Reset();
	}
}

void CTexture::LoadTextureFromFile(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const wchar_t* pszFileName, UINT nResourceType, UINT nIndex)
{
	m_pnResourceTypes[nIndex] = nResourceType;
	m_ppd3dTextures[nIndex] = ::CreateTextureResourceFromDDSFile(
		pd3dDevice,
		pd3dCommandList,
		pszFileName,
		&m_ppd3dTextureUploadBuffers[nIndex],
		D3D12_RESOURCE_STATE_GENERIC_READ/*D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE*/
	);
}

ID3D12Resource* CTexture::CreateTexture(ID3D12Device* pd3dDevice, UINT nWidth, UINT nHeight, DXGI_FORMAT dxgiFormat, D3D12_RESOURCE_FLAGS d3dResourceFlags, D3D12_RESOURCE_STATES d3dResourceStates, D3D12_CLEAR_VALUE* pd3dClearValue, UINT nResourceType, UINT nIndex)
{
	m_pnResourceTypes[nIndex] = nResourceType;
	m_ppd3dTextures[nIndex] = ::CreateTexture2DResource(
		pd3dDevice,
		nWidth,
		nHeight,
		1,
		0,
		dxgiFormat,
		d3dResourceFlags,
		d3dResourceStates,
		pd3dClearValue
	);
	return(m_ppd3dTextures[nIndex].Get());
}

void CTexture::LoadBuffer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, void* pData, UINT nElements, UINT nStride, DXGI_FORMAT ndxgiFormat, UINT nIndex)
{
	m_pnResourceTypes[nIndex] = RESOURCE_BUFFER;
	m_pdxgiBufferFormats[nIndex] = ndxgiFormat;
	m_pnBufferElements[nIndex] = nElements;
	m_ppd3dTextures[nIndex] = ::CreateBufferResource(
		pd3dDevice,
		pd3dCommandList,
		pData,
		nElements * nStride,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&m_ppd3dTextureUploadBuffers[nIndex]
	);
}

D3D12_SHADER_RESOURCE_VIEW_DESC CTexture::GetShaderResourceViewDesc(int nIndex)
{
	ID3D12Resource* pShaderResource = GetResource(nIndex);
	if (!pShaderResource)
	{
		OutputDebugStringA("[Texture] ERROR: GetShaderResourceViewDesc: resource is null\n");
		D3D12_SHADER_RESOURCE_VIEW_DESC dummy{};
		dummy.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		dummy.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dummy.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		dummy.Texture2D.MipLevels = 1;
		return dummy;
	}

	D3D12_RESOURCE_DESC rd = pShaderResource->GetDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC desc{}; //
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	int nTextureType = GetTextureType(nIndex);
	switch (nTextureType)
	{
	case RESOURCE_TEXTURE2D:
	case RESOURCE_TEXTURE2D_ARRAY:
		desc.Format = rd.Format;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.MipLevels = rd.MipLevels;
		desc.Texture2D.PlaneSlice = 0;
		desc.Texture2D.ResourceMinLODClamp = 0.0f;
		break;

	case RESOURCE_TEXTURE2DARRAY:
		desc.Format = rd.Format;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MostDetailedMip = 0;
		desc.Texture2DArray.MipLevels = rd.MipLevels;
		desc.Texture2DArray.PlaneSlice = 0;
		desc.Texture2DArray.ResourceMinLODClamp = 0.0f;
		desc.Texture2DArray.FirstArraySlice = 0;
		desc.Texture2DArray.ArraySize = rd.DepthOrArraySize;
		break;

	case RESOURCE_TEXTURE_CUBE:
		desc.Format = rd.Format;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		desc.TextureCube.MostDetailedMip = 0;
		desc.TextureCube.MipLevels = rd.MipLevels;
		desc.TextureCube.ResourceMinLODClamp = 0.0f;
		break;

	case RESOURCE_BUFFER:
		desc.Format = m_pdxgiBufferFormats[nIndex];
		desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		desc.Buffer.FirstElement = 0;
		desc.Buffer.NumElements = m_pnBufferElements[nIndex];
		desc.Buffer.StructureByteStride = 0;
		desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		break;
	}

	return desc;
}

std::wstring ResolveTexturePath(
	const std::string& assetName,
	const std::string& texBaseName)
{
	// ---- string(UTF-8 가정) -> wstring 변환 람다 ----
	auto Utf8ToWString = [](const std::string& s) -> std::wstring
		{
			if (s.empty()) return std::wstring();

			int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
			if (len <= 0) return std::wstring();

			std::wstring ws;
			ws.resize(static_cast<size_t>(len - 1)); // -1: null 제외
			MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws.data(), len);
			return ws;
		};

	std::wstring wAsset = Utf8ToWString(assetName);
	std::wstring wTex = Utf8ToWString(texBaseName);

	// "Assets/<assetName>/Texture/<texBaseName>.dds"
	std::wstring out = L"Assets/";
	out += wAsset;
	out += L"/Texture/";
	out += wTex;
	out += L".dds";
	return out;
}