//-----------------------------------------------------------------------------
// File: HeightMapImage.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "HeightMapImage.h"

HeightMapImage::HeightMapImage(LPCTSTR pFileName, int nWidth, int nLength, XMFLOAT3 xmf3Scale)
{
	m_nWidth = nWidth;
	m_nLength = nLength;
	m_xmf3Scale = xmf3Scale;

	BYTE* pHeightMapPixels = new BYTE[m_nWidth * m_nLength];

	//파일을 열고 읽는다. 높이 맵 이미지는 파일 헤더가 없는 RAW 이미지이다.
	HANDLE hFile = ::CreateFile(pFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY, NULL);
	DWORD dwBytesRead;
	::ReadFile(hFile, pHeightMapPixels, (m_nWidth * m_nLength), &dwBytesRead, NULL);
	::CloseHandle(hFile);

	m_pHeightMapPixels = new BYTE[m_nWidth * m_nLength];
	for (int y = 0; y < m_nLength; y++)
	{
		for (int x = 0; x < m_nWidth; x++)
		{
			m_pHeightMapPixels[x + ((m_nLength - 1 - y) * m_nWidth)] = pHeightMapPixels[x +
				(y * m_nWidth)];
		}
	}
	if (pHeightMapPixels) delete[] pHeightMapPixels;
}

HeightMapImage::~HeightMapImage()
{
	if (m_pHeightMapPixels) delete[] m_pHeightMapPixels;
	m_pHeightMapPixels = NULL;
}

#define _WITH_APPROXIMATE_OPPOSITE_CORNER

float HeightMapImage::GetHeight(float fx, float fz, bool bReverseQuad)
{
	fx = fx / m_xmf3Scale.x;
	fz = fz / m_xmf3Scale.z;

	if ( ( fx < 0.0f ) || ( fz < 0.0f ) || ( fx > static_cast< float >(m_nWidth - 1) ) || ( fz > static_cast< float >( m_nLength - 1 ) ) )
		return 0.0f;

	const float maxFx = static_cast< float >( m_nWidth - 1 ) - 0.001f;
	const float maxFz = static_cast< float >( m_nLength - 1 ) - 0.001f;

	if ( fx > maxFx )
		fx = maxFx;

	if ( fz > maxFz )
		fz = maxFz;

	int x = static_cast< int >( fx );
	int z = static_cast< int >( fz );
	float fxPercent = fx - x;
	float fzPercent = fz - z;

	float fBottomLeft = static_cast< float >( m_pHeightMapPixels[x + ( z * m_nWidth )] );
	float fBottomRight = static_cast< float >( m_pHeightMapPixels[( x + 1 ) + ( z * m_nWidth )] );
	float fTopLeft = static_cast< float >( m_pHeightMapPixels[x + ( ( z + 1 ) * m_nWidth )] );
	float fTopRight = static_cast< float >( m_pHeightMapPixels[( x + 1 ) + ( ( z + 1 ) * m_nWidth )] );

#ifdef _WITH_APPROXIMATE_OPPOSITE_CORNER
	if ( bReverseQuad )
	{
		if ( fzPercent >= fxPercent )
			fBottomRight = fBottomLeft + ( fTopRight - fTopLeft );
		else
			fTopLeft = fTopRight + ( fBottomLeft - fBottomRight );
	}
	else
	{
		if ( fzPercent < ( 1.0f - fxPercent ) )
			fTopRight = fTopLeft + ( fBottomRight - fBottomLeft );
		else
			fBottomLeft = fTopLeft + ( fBottomRight - fTopRight );
	}
#endif

	float fTopHeight = fTopLeft * ( 1.0f - fxPercent ) + fTopRight * fxPercent;
	float fBottomHeight = fBottomLeft * ( 1.0f - fxPercent ) + fBottomRight * fxPercent;
	float fHeight = fBottomHeight * ( 1.0f - fzPercent ) + fTopHeight * fzPercent;

	return fHeight;
}

XMFLOAT3 HeightMapImage::GetHeightMapNormal(int x, int z)
{
	if ((x < 0.0f) || (z < 0.0f) || (x >= m_nWidth) || (z >= m_nLength))
		return(XMFLOAT3(0.0f, 1.0f, 0.0f));

	int nHeightMapIndex = x + (z * m_nWidth);
	int xHeightMapAdd = (x < (m_nWidth - 1)) ? 1 : -1;
	int zHeightMapAdd = (z < (m_nLength - 1)) ? m_nWidth : -m_nWidth;

	float y1 = (float)m_pHeightMapPixels[nHeightMapIndex] * m_xmf3Scale.y;
	float y2 = (float)m_pHeightMapPixels[nHeightMapIndex + xHeightMapAdd] * m_xmf3Scale.y;
	float y3 = (float)m_pHeightMapPixels[nHeightMapIndex + zHeightMapAdd] * m_xmf3Scale.y;
	XMFLOAT3 xmf3Edge1 = XMFLOAT3(0.0f, y3 - y1, m_xmf3Scale.z);
	XMFLOAT3 xmf3Edge2 = XMFLOAT3(m_xmf3Scale.x, y2 - y1, 0.0f);
	XMFLOAT3 xmf3Normal = Vector3::CrossProduct(xmf3Edge1, xmf3Edge2, true);

	return(xmf3Normal);
}