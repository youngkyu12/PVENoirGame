#pragma once

#define RESOURCE_TEXTURE2D			0x01
#define RESOURCE_TEXTURE2D_ARRAY	0x02	//[]
#define RESOURCE_TEXTURE2DARRAY		0x03
#define RESOURCE_TEXTURE_CUBE		0x04
#define RESOURCE_BUFFER				0x05

class CTexture
{
public:
	CTexture(int nTextureResources, UINT nResourceType, int nSamplers, int nRootParameters);
	virtual ~CTexture();

private:
	UINT							m_nTextureType = RESOURCE_TEXTURE2D;

	int								m_nTextures = 0;
	vector<ComPtr<ID3D12Resource>>	m_ppd3dTextures;
	vector<ComPtr<ID3D12Resource>>	m_ppd3dTextureUploadBuffers;

	unique_ptr<UINT[]>				m_pnResourceTypes;

	unique_ptr<DXGI_FORMAT[]>		m_pdxgiBufferFormats;
	unique_ptr<int[]>				m_pnBufferElements;

	int								m_nRootParameters = 0;
	unique_ptr<int[]>				m_pnRootParameterIndices;
	unique_ptr<D3D12_GPU_DESCRIPTOR_HANDLE[]> m_pd3dSrvGpuDescriptorHandles;

	int								m_nSamplers = 0;
	unique_ptr<D3D12_GPU_DESCRIPTOR_HANDLE[]> m_pd3dSamplerGpuDescriptorHandles;

public:
	void SetSampler(int nIndex, D3D12_GPU_DESCRIPTOR_HANDLE d3dSamplerGpuDescriptorHandle);

	void UpdateShaderVariable(ID3D12GraphicsCommandList* pd3dCommandList, int nParameterIndex, int nTextureIndex);
	void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);
	void ReleaseShaderVariables();

	void LoadTextureFromFile(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const wchar_t* pszFileName, UINT nResourceType, UINT nIndex);
	void LoadBuffer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, void* pData, UINT nElements, UINT nStride, DXGI_FORMAT ndxgiFormat, UINT nIndex);
	ID3D12Resource* CreateTexture(ID3D12Device* pd3dDevice, UINT nWidth, UINT nHeight, DXGI_FORMAT dxgiFormat, D3D12_RESOURCE_FLAGS d3dResourceFlags, D3D12_RESOURCE_STATES d3dResourceStates, D3D12_CLEAR_VALUE* pd3dClearValue, UINT nResourceType, UINT nIndex);

	void SetRootParameterIndex(int nIndex, UINT nRootParameterIndex);
	int GetRootParameterIndex(int nIndex) { return(m_pnRootParameterIndices[nIndex]); }
	void SetGpuDescriptorHandle(int nIndex, D3D12_GPU_DESCRIPTOR_HANDLE d3dSrvGpuDescriptorHandle);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuDescriptorHandle(int nIndex) { return(m_pd3dSrvGpuDescriptorHandles[nIndex]); }

	int GetRootParameters() { return(m_nRootParameters); }
	int GetTextures() { return(m_nTextures); }
	ID3D12Resource* GetResource(int nIndex) { return(m_ppd3dTextures[nIndex].Get()); }

	UINT GetTextureType() { return(m_nTextureType); }
	UINT GetTextureType(int nIndex) { return(m_pnResourceTypes[nIndex]); }
	DXGI_FORMAT GetBufferFormat(int nIndex) { return(m_pdxgiBufferFormats[nIndex]); }
	int GetBufferElements(int nIndex) { return(m_pnBufferElements[nIndex]); }

	D3D12_SHADER_RESOURCE_VIEW_DESC GetShaderResourceViewDesc(int nIndex);

	void ReleaseUploadBuffers();
};