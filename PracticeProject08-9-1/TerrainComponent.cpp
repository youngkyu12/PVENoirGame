#include "stdafx.h"
#include "TerrainComponent.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "ModelComponent.h"
#include "Object.h"
#include "Shader.h"

TerrainComponent::TerrainComponent(
	CGameObject* owner,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	LPCTSTR pFileName,
	int nWidth,
	int nLength,
	int nBlockWidth,
	int nBlockLength,
	XMFLOAT3 xmf3Scale,
	XMFLOAT4 xmf4Color)
	: CComponentT<TerrainComponent>(owner)
	, m_pd3dGraphicsRootSignature(pd3dGraphicsRootSignature)
	, m_nWidth(nWidth)
	, m_nLength(nLength)
	, m_nBlockWidth(nBlockWidth)
	, m_nBlockLength(nBlockLength)
	, m_xmf3Scale(xmf3Scale)
	, m_xmf4Color(xmf4Color)
{
	if (pFileName)
		m_heightMapPath = pFileName;
}

void TerrainComponent::OnCreate(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	mModel = GetOwner()->GetComponent<CModelComponent>();
	assert(mModel && "TerrainComponent requires CModelComponent");
	if (!mModel)
		return;

	if (!LoadHeightMapRaw(m_heightMapPath.c_str(), m_nWidth, m_nLength))
	{
		m_heightMapPixels.assign(static_cast<size_t>(m_nWidth * m_nLength), static_cast<BYTE>(0));
	}

	const int cxQuadsPerBlock = max(1, m_nBlockWidth - 1);
	const int czQuadsPerBlock = max(1, m_nBlockLength - 1);

	const int cxBlocks = max(1, (m_nWidth - 1) / cxQuadsPerBlock);
	const int czBlocks = max(1, (m_nLength - 1) / czQuadsPerBlock);

	const int nMeshes = cxBlocks * czBlocks;
	mModel->Resize(nMeshes);

	for (int z = 0; z < czBlocks; ++z)
	{
		for (int x = 0; x < cxBlocks; ++x)
		{
			const int xStart = x * cxQuadsPerBlock;
			const int zStart = z * czQuadsPerBlock;

			shared_ptr<CHeightMapGridMesh> mesh = std::make_shared<CHeightMapGridMesh>(
				pd3dDevice,
				pd3dCommandList,
				xStart,
				zStart,
				m_nBlockWidth,
				m_nBlockLength,
				m_xmf3Scale,
				m_xmf4Color,
				this);
			
			mModel->SetMesh(x + (z * cxBlocks), std::move(mesh));
		}
	}
	// 밖에서 할 것
	std::shared_ptr<CTerrainShader> terrainShader = std::make_shared<CTerrainShader>();
	DXGI_FORMAT rtvFormats[5] =
	{
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R32_FLOAT
	};
	terrainShader->CreateShader(
		pd3dDevice,
		m_pd3dGraphicsRootSignature,
		5,
		rtvFormats,
		DXGI_FORMAT_D24_UNORM_S8_UINT);
	GetOwner()->SetShader(terrainShader);
}

float TerrainComponent::GetHeight(float x, float z) const
{
	return SampleHeightMap(
		static_cast<int>(x / max(0.0001f, m_xmf3Scale.x)),
		static_cast<int>(z / max(0.0001f, m_xmf3Scale.z)));
}

XMFLOAT3 TerrainComponent::GetNormal(float x, float z) const
{
	return SampleHeightMapNormal(
		static_cast<int>(x / max(0.0001f, m_xmf3Scale.x)),
		static_cast<int>(z / max(0.0001f, m_xmf3Scale.z)));
}

float TerrainComponent::SampleHeightMap(int x, int z) const
{
	return SampleHeightMapRaw01(x, z) * m_xmf3Scale.y;
}

XMFLOAT3 TerrainComponent::SampleHeightMapNormal(int x, int z) const
{
	const float hL = SampleHeightMap(x - 1, z);
	const float hR = SampleHeightMap(x + 1, z);
	const float hD = SampleHeightMap(x, z - 1);
	const float hU = SampleHeightMap(x, z + 1);

	const XMFLOAT3 dx(2.0f * m_xmf3Scale.x, hR - hL, 0.0f);
	const XMFLOAT3 dz(0.0f, hU - hD, 2.0f * m_xmf3Scale.z);
	return Vector3::Normalize(Vector3::CrossProduct(dz, dx, false));
}

bool TerrainComponent::LoadHeightMapRaw(LPCTSTR pFileName, int nWidth, int nLength)
{
	if (!pFileName || nWidth <= 0 || nLength <= 0)
		return false;

	const size_t pixelCount = static_cast<size_t>(nWidth) * static_cast<size_t>(nLength);
	m_heightMapPixels.assign(pixelCount, static_cast<BYTE>(0));

	const std::filesystem::path path(pFileName);
	std::ifstream fin(path, std::ios::binary);
	if (!fin.is_open())
		return false;

	fin.read(reinterpret_cast<char*>(m_heightMapPixels.data()), static_cast<std::streamsize>(pixelCount));
	return fin.good() || fin.eof();
}

float TerrainComponent::SampleHeightMapRaw01(int x, int z) const
{
	if (m_heightMapPixels.empty() || m_nWidth <= 0 || m_nLength <= 0)
		return 0.0f;

	x = ClampX(x);
	z = ClampZ(z);

	const size_t idx = static_cast<size_t>(x) + (static_cast<size_t>(z) * static_cast<size_t>(m_nWidth));
	if (idx >= m_heightMapPixels.size())
		return 0.0f;
	return static_cast<float>(m_heightMapPixels[idx]);
}

int TerrainComponent::ClampX(int x) const
{
	return std::clamp(x, 0, max(0, m_nWidth - 1));
}

int TerrainComponent::ClampZ(int z) const
{
	return std::clamp(z, 0, max(0, m_nLength - 1));
}