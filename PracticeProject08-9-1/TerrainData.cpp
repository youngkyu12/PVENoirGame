#include "stdafx.h"
#include "TerrainData.h"
#include "HeightMapImage.h"

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

float TerrainData::GetHeight(float localX, float loaclZ) const
{
	if (!m_heightMapImage)
		return 0.0f;

	return m_heightMapImage->GetHeight(
		localX / m_xmf3Scale.x,
		loaclZ / m_xmf3Scale.z) * m_xmf3Scale.y;
}

XMFLOAT3 TerrainData::GetNormal(float localX, float loaclZ) const
{
	if (!m_heightMapImage)
		return XMFLOAT3(0.0f, 1.0f, 0.0f);

	return m_heightMapImage->GetHeightMapNormal(
		int(localX / m_xmf3Scale.x),
		int(loaclZ / m_xmf3Scale.z));
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
	return m_nWidth * m_xmf3Scale.x;
}

float TerrainData::GetWorldLength() const
{
	return m_nLength * m_xmf3Scale.z;
}

int TerrainData::GetnBlockWidth() const
{
	return m_nBlockWidth;
}

int TerrainData::GetnBlockLength() const
{
	return m_nBlockLength;
}

HeightMapImage* TerrainData::GetHeightMapImage() const
{
	return m_heightMapImage;
}