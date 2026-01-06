//-----------------------------------------------------------------------------
// File: CGameObject.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Object.h"
#include "Shader.h"
#include "Texture.h"
#include "Material.h"
#include "Scene.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CGameObject::CGameObject(int nMeshes)
{
	m_nMeshes = nMeshes;
	if (m_nMeshes > 0)
	{
		m_ppMeshes.resize(m_nMeshes);
	}
}

CGameObject::~CGameObject()
{
	ReleaseShaderVariables();

	if (!m_ppMeshes.empty())
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i])
				m_ppMeshes[i].reset();
		}
	}
}

void CGameObject::ReleaseShaderVariables()
{
	if (m_pd3dcbGameObject)
	{
		m_pd3dcbGameObject->Unmap(0, nullptr);
		m_pd3dcbGameObject.Reset();
	}

	if (m_pMaterial)
		m_pMaterial->ReleaseShaderVariables();
}

void CGameObject::ReleaseUploadBuffers()
{
	if (!m_ppMeshes.empty())
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i])m_ppMeshes[i]->ReleaseUploadBuffers();
		}
	}

	if (m_pMaterial)m_pMaterial->ReleaseUploadBuffers();
}

void CGameObject::SetMesh(int nIndex, shared_ptr<CMesh> pMesh)
{
	if (!m_ppMeshes.empty())
	{
		if (m_ppMeshes[nIndex])
			m_ppMeshes[nIndex].reset();
		m_ppMeshes[nIndex] = pMesh;
	}
}

void CGameObject::SetMesh(int nIndex, shared_ptr<CTerrainMesh> pMesh)
{
	if (!m_ppTerrainMeshes.empty())
	{
		if (m_ppTerrainMeshes[nIndex])
			m_ppTerrainMeshes[nIndex].reset();
		m_ppTerrainMeshes[nIndex] = pMesh;
	}
}

void CGameObject::SetShader(shared_ptr<CShader> pShader)
{
	if (!m_pMaterial)
	{
		shared_ptr<CMaterial> pMaterial = make_shared<CMaterial>();
		SetMaterial(pMaterial);
	}
	if (m_pMaterial)
		m_pMaterial->SetShader(pShader);
}

void CGameObject::SetMaterial(shared_ptr<CMaterial> pMaterial)
{
	if (m_pMaterial)
		m_pMaterial.reset();
	m_pMaterial = pMaterial;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
CRotatingObject::CRotatingObject(int nMeshes)
{
}

CRotatingObject::~CRotatingObject()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
CRevolvingObject::CRevolvingObject(int nMeshes)
{
}

CRevolvingObject::~CRevolvingObject()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
CHeightMapTerrain::CHeightMapTerrain(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, LPCTSTR pFileName, int nWidth, int nLength, int nBlockWidth, int nBlockLength, XMFLOAT3 xmf3Scale, XMFLOAT4 xmf4Color)
{
	m_nWidth = nWidth;
	m_nLength = nLength;

	int cxQuadsPerBlock = nBlockWidth - 1;
	int czQuadsPerBlock = nBlockLength - 1;

	m_xmf3Scale = xmf3Scale;

	m_pHeightMapImage = make_unique<CHeightMapImage>(pFileName, nWidth, nLength, xmf3Scale);

	long cxBlocks = (m_nWidth - 1) / cxQuadsPerBlock;
	long czBlocks = (m_nLength - 1) / czQuadsPerBlock;

	m_nMeshes = cxBlocks * czBlocks;
	m_ppMeshes.resize(m_nMeshes);

	for (int z = 0, zStart = 0; z < czBlocks; z++)
	{
		for (int x = 0, xStart = 0; x < cxBlocks; x++)
		{
			xStart = x * (nBlockWidth - 1);
			zStart = z * (nBlockLength - 1);
			shared_ptr<CHeightMapGridMesh> pHeightMapGridMesh = make_shared<CHeightMapGridMesh>(pd3dDevice, pd3dCommandList, xStart, zStart, nBlockWidth, nBlockLength, xmf3Scale, xmf4Color, m_pHeightMapImage.get());
			SetMesh(x + (z * cxBlocks), pHeightMapGridMesh);
		}
	}

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	shared_ptr<CTexture> pTerrainTexture = make_shared<CTexture>(5, RESOURCE_TEXTURE2D, 0, 1);

	pTerrainTexture->LoadTextureFromFile(pd3dDevice, pd3dCommandList, L"Image/Base_Texture.dds", RESOURCE_TEXTURE2D, 0);
	pTerrainTexture->LoadTextureFromFile(pd3dDevice, pd3dCommandList, L"Image/Detail_Texture_7.dds", RESOURCE_TEXTURE2D, 1);
	pTerrainTexture->LoadTextureFromFile(pd3dDevice, pd3dCommandList, L"Image/Detail_Texture_1.dds", RESOURCE_TEXTURE2D, 2);
	pTerrainTexture->LoadTextureFromFile(pd3dDevice, pd3dCommandList, L"Image/Lava(Diffuse).dds", RESOURCE_TEXTURE2D, 3);
	//	pTerrainTexture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, L"Image/HeightMap-Alpha(Flipped).dds", RESOURCE_TEXTURE2D, 4);
	pTerrainTexture->LoadTextureFromFile(pd3dDevice, pd3dCommandList, L"Image/HeightMap2(Flipped)Alpha.dds", RESOURCE_TEXTURE2D, 4);
	CScene::m_pDescriptorHeap->CreateShaderResourceViews(pd3dDevice, pTerrainTexture.get(), 0, 4);
	
	UINT ncbElementBytes = ((sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255); //256의 배수

	DXGI_FORMAT pdxgiRtvFormats = DXGI_FORMAT_R8G8B8A8_UNORM;
	shared_ptr<CTerrainShader> pTerrainShader = make_shared<CTerrainShader>();
	pTerrainShader->CreateShader(pd3dDevice, pd3dGraphicsRootSignature, 1, &pdxgiRtvFormats, DXGI_FORMAT_D24_UNORM_S8_UINT);
	pTerrainShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	D3D12_GPU_DESCRIPTOR_HANDLE d3dCbvGPUDescriptorHandle = CScene::m_pDescriptorHeap->CreateConstantBufferView(pd3dDevice, m_pd3dcbGameObject.Get(), ncbElementBytes);
	SetCbvGPUDescriptorHandle(d3dCbvGPUDescriptorHandle);

	shared_ptr<CMaterial> pTerrainMaterial = make_shared<CMaterial>();
	pTerrainMaterial->SetTexture(pTerrainTexture);

	SetMaterial(pTerrainMaterial);
	SetShader(pTerrainShader);
}

CHeightMapTerrain::~CHeightMapTerrain()
{
}

CTerrainWater::CTerrainWater(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, float nWidth, float nLength)
	: CGameObject(1)
{
	shared_ptr<CTexturedRectMesh> pWaterMesh = make_shared<CTexturedRectMesh>(pd3dDevice, pd3dCommandList, nWidth, 0.0f, nLength, 0.0f, 0.0f, 0.0f);
	SetMesh(0, pWaterMesh);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	shared_ptr<CTexture> pWaterTexture = make_shared<CTexture>(3, RESOURCE_TEXTURE2D, 0, 1);

	pWaterTexture->LoadTextureFromFile(pd3dDevice, pd3dCommandList, L"Image/Water_Base_Texture_0.dds", RESOURCE_TEXTURE2D, 0);
	pWaterTexture->LoadTextureFromFile(pd3dDevice, pd3dCommandList, L"Image/Water_Detail_Texture_0.dds", RESOURCE_TEXTURE2D, 1);
	pWaterTexture->LoadTextureFromFile(pd3dDevice, pd3dCommandList, L"Image/Lava(Diffuse).dds", RESOURCE_TEXTURE2D, 2);
	//	pWaterTexture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, L"Image/Water_Texture_Alpha.dds", RESOURCE_TEXTURE2D, 2);
	CScene::m_pDescriptorHeap->CreateShaderResourceViews(pd3dDevice, pWaterTexture.get(), 0, 5);

	UINT ncbElementBytes = ((sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255); //256의 배수

	DXGI_FORMAT pdxgiRtvFormats = DXGI_FORMAT_R8G8B8A8_UNORM;
	shared_ptr<CTerrainWaterShader> pWaterShader = make_shared<CTerrainWaterShader>();
	pWaterShader->CreateShader(pd3dDevice, pd3dGraphicsRootSignature, 1, &pdxgiRtvFormats, DXGI_FORMAT_D24_UNORM_S8_UINT);
	pWaterShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	D3D12_GPU_DESCRIPTOR_HANDLE d3dCbvGPUDescriptorHandle = CScene::m_pDescriptorHeap->CreateConstantBufferView(pd3dDevice, m_pd3dcbGameObject.Get(), ncbElementBytes);
	SetCbvGPUDescriptorHandle(d3dCbvGPUDescriptorHandle);

	shared_ptr<CMaterial> pWaterMaterial = make_shared<CMaterial>();
	pWaterMaterial->SetTexture(pWaterTexture);

	SetMaterial(pWaterMaterial);
	SetShader(pWaterShader);
}

CTerrainWater::~CTerrainWater()
{
}
