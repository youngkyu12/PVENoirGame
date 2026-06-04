//-----------------------------------------------------------------------------
// File: TerrainData.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "TerrainData.h"
#include "HeightMapImage.h"
#include "Texture.h"

TerrainData::TerrainData(
	LPCTSTR pFileName, int nWidth, int nLength, int nBlockWidth, int nBlockLength, 
	XMFLOAT3 xmf3Scale, XMFLOAT4 xmf4Color)
{
	// 지형에 사용할 높이 맵의 가로, 세로의 크기이다.
	m_nWidth = nWidth;
	m_nLength = nLength;

	m_nBlockWidth = nBlockWidth;
	m_nBlockLength = nBlockLength;

	// xmf3Scale는 지형을 실제로 몇 배 확대할 것인가를 나타낸다.
	m_xmf3Scale = xmf3Scale;

	// 지형에 사용할 높이 맵을 생성한다.
	m_heightMapImage = new HeightMapImage(pFileName, nWidth, nLength, xmf3Scale);
}

void TerrainData::SetHeightMapImage(HeightMapImage* heightMapImage)
{
	m_heightMapImage = heightMapImage;
}

void TerrainData::SetSize(int width, int length)
{
	m_nWidth = width;
	m_nLength = length;
}

void TerrainData::SetScale(const XMFLOAT3& scale)
{
	m_xmf3Scale = scale;
}

void TerrainData::SetWorldPosition(const XMFLOAT3& position)
{
	m_xmf3WorldPosition = position;
}

XMFLOAT3 TerrainData::GetWorldPosition() const
{
	return m_xmf3WorldPosition;
}

float TerrainData::GetHeight(float localX, float localZ, bool bReverseQuad) const
{
	if ( !m_heightMapImage )
		return 0.0f;

	return m_heightMapImage->GetHeight(localX, localZ, bReverseQuad) * m_xmf3Scale.y;
}

XMFLOAT3 TerrainData::GetNormal(float localX, float localZ) const
{
	if ( !m_heightMapImage )
		return XMFLOAT3(0.0f, 1.0f, 0.0f);

	return m_heightMapImage->GetHeightMapNormal(static_cast< int >( localX / m_xmf3Scale.x ), static_cast< int >( localZ / m_xmf3Scale.z ));
}

int TerrainData::GetHeightMapWidth() const
{
	if (!m_heightMapImage)
		return 0;

	return m_heightMapImage->GetHeightMapWidth();
}

int TerrainData::GetHeightMapLength() const
{
	if (!m_heightMapImage)
		return 0;

	return m_heightMapImage->GetHeightMapLength();
}

XMFLOAT3 TerrainData::GetScale() const
{
	return m_xmf3Scale;
}

int TerrainData::GetWidthCount() const
{
	return m_nWidth;
}

int TerrainData::GetLengthCount() const
{
	return m_nLength;
}

float TerrainData::GetWorldWidth() const
{
	return ( m_nWidth > 1 ) ? float(m_nWidth - 1) * m_xmf3Scale.x : 0.0f;
}

float TerrainData::GetWorldLength() const
{
	return ( m_nLength > 1 ) ? float(m_nLength - 1) * m_xmf3Scale.z : 0.0f;
}

int TerrainData::GetnBlockWidth() const
{
	return m_nBlockWidth;
}

int TerrainData::GetnBlockLength() const
{
	return m_nBlockLength;
}

void TerrainData::SetHeightMapTexture(std::shared_ptr<CTexture> texture)
{
	m_heightMapTexture = texture;
	srvHeightMapIndex = texture ? texture->GetBaseSrvIndex() : UINT_MAX;
}

void TerrainData::SetHeightMapSrvIndex(UINT srvIndex)
{
	srvHeightMapIndex = srvIndex;
}

UINT TerrainData::GetsrvIndex() const
{
	return srvHeightMapIndex;
}

HeightMapImage* TerrainData::GetHeightMapImage() const
{
	return m_heightMapImage;
}

void TerrainData::SetGrassDiffuseTexture(std::shared_ptr<CTexture> texture)
{
	m_grassDiffuseTexture = texture;
	srvGrassDiffuseIndex = texture ? texture->GetBaseSrvIndex() : UINT_MAX;
}

void TerrainData::SetGroundDiffuseTexture(std::shared_ptr<CTexture> texture)
{
	m_groundDiffuseTexture = texture;
	srvGroundDiffuseIndex = texture ? texture->GetBaseSrvIndex() : UINT_MAX;
}

void TerrainData::SetDirtDiffuseTexture(std::shared_ptr<CTexture> texture)
{
	m_dirtDiffuseTexture = texture;
	srvDirtDiffuseIndex = texture ? texture->GetBaseSrvIndex() : UINT_MAX;
}

void TerrainData::SetGrassNormalTexture(std::shared_ptr<CTexture> texture)
{
	m_grassNormalTexture = texture;
	srvGrassNormalIndex = texture ? texture->GetBaseSrvIndex() : UINT_MAX;
}

void TerrainData::SetGroundNormalTexture(std::shared_ptr<CTexture> texture)
{
	m_groundNormalTexture = texture;
	srvGroundNormalIndex = texture ? texture->GetBaseSrvIndex() : UINT_MAX;
}

void TerrainData::SetDirtNormalTexture(std::shared_ptr<CTexture> texture)
{
	m_dirtNormalTexture = texture;
	srvDirtNormalIndex = texture ? texture->GetBaseSrvIndex() : UINT_MAX;
}

UINT TerrainData::GetGrassDiffuseSrvIndex() const
{
	return srvGrassDiffuseIndex;
}

UINT TerrainData::GetGroundDiffuseSrvIndex() const
{
	return srvGroundDiffuseIndex;
}

UINT TerrainData::GetDirtDiffuseSrvIndex() const
{
	return srvDirtDiffuseIndex;
}

UINT TerrainData::GetGrassNormalSrvIndex() const
{
	return srvGrassNormalIndex;
}

UINT TerrainData::GetGroundNormalSrvIndex() const
{
	return srvGroundNormalIndex;
}

UINT TerrainData::GetDirtNormalSrvIndex() const
{
	return srvDirtNormalIndex;
}