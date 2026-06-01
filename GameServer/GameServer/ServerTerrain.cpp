#include "pch.h"
#include "ServerTerrain.h"

#include <cmath>
#include <fstream>

bool CServerTerrain::LoadFromFile(const std::string& path)
{
	m_loaded = false;
	m_pixels.clear();

	std::ifstream fin(path, std::ios::binary);
	if (!fin.is_open())
		return false;

	constexpr size_t kPixelCount =
		static_cast<size_t>(kTerrainSamples) * static_cast<size_t>(kTerrainSamples);

	std::vector<uint8_t> raw(kPixelCount, 0);
	fin.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
	if (fin.gcount() != static_cast<std::streamsize>(raw.size()))
		return false;

	m_pixels.assign(kPixelCount, 0);
	for (int y = 0; y < kTerrainSamples; ++y)
	{
		for (int x = 0; x < kTerrainSamples; ++x)
		{
			const int src = x + (y * kTerrainSamples);
			const int dst = x + ((kTerrainSamples - 1 - y) * kTerrainSamples);
			m_pixels[static_cast<size_t>(dst)] = raw[static_cast<size_t>(src)];
		}
	}

	m_loaded = true;
	return true;
}

bool CServerTerrain::Contains(float worldX, float worldZ) const
{
	const float maxX = kTerrainOriginX + (kTerrainTileCols * kTerrainTileSize);
	const float maxZ = kTerrainOriginZ + (kTerrainTileRows * kTerrainTileSize);
	return worldX >= kTerrainOriginX && worldX < maxX &&
		worldZ >= kTerrainOriginZ && worldZ < maxZ;
}

float CServerTerrain::SampleHeight(float worldX, float worldZ) const
{
	if (!m_loaded || !Contains(worldX, worldZ))
		return 0.0f;

	const int tileX = static_cast<int>(std::floor((worldX - kTerrainOriginX) / kTerrainTileSize));
	const int tileZ = static_cast<int>(std::floor((worldZ - kTerrainOriginZ) / kTerrainTileSize));

	const float tileOriginX = kTerrainOriginX + (static_cast<float>(tileX) * kTerrainTileSize);
	const float tileOriginZ = kTerrainOriginZ + (static_cast<float>(tileZ) * kTerrainTileSize);
	const float localX = worldX - tileOriginX;
	const float localZ = worldZ - tileOriginZ;

	const bool reverseQuad = (static_cast<int>(std::floor(localZ / kTerrainScale)) % 2) != 0;
	return kTerrainPlacementY + SampleLocalHeight(localX, localZ, reverseQuad);
}

float CServerTerrain::SampleLocalHeight(float localX, float localZ, bool reverseQuad) const
{
	const float fx = localX / kTerrainScale;
	const float fz = localZ / kTerrainScale;

	if (fx < 0.0f || fz < 0.0f || fx >= kTerrainSamples || fz >= kTerrainSamples)
		return 0.0f;

	const int x = static_cast<int>(fx);
	const int z = static_cast<int>(fz);
	if (x < 0 || z < 0 || x >= (kTerrainSamples - 1) || z >= (kTerrainSamples - 1))
		return 0.0f;

	const float fxPercent = fx - static_cast<float>(x);
	const float fzPercent = fz - static_cast<float>(z);

	float bottomLeft = static_cast<float>(SamplePixel(x, z));
	float bottomRight = static_cast<float>(SamplePixel(x + 1, z));
	float topLeft = static_cast<float>(SamplePixel(x, z + 1));
	float topRight = static_cast<float>(SamplePixel(x + 1, z + 1));

	if (reverseQuad)
	{
		if (fzPercent >= fxPercent)
			bottomRight = bottomLeft + (topRight - topLeft);
		else
			topLeft = topRight + (bottomLeft - bottomRight);
	}
	else
	{
		if (fzPercent < (1.0f - fxPercent))
			topRight = topLeft + (bottomRight - bottomLeft);
		else
			bottomLeft = topLeft + (bottomRight - topRight);
	}

	const float topHeight = topLeft * (1.0f - fxPercent) + topRight * fxPercent;
	const float bottomHeight = bottomLeft * (1.0f - fxPercent) + bottomRight * fxPercent;
	return bottomHeight * (1.0f - fzPercent) + topHeight * fzPercent;
}

uint8_t CServerTerrain::SamplePixel(int x, int z) const
{
	return m_pixels[static_cast<size_t>(x + (z * kTerrainSamples))];
}
